#include "engine/level.h"

#include "blocks/block-behaviour.h"
#include "blocks/handlers.h"
#include "core/trace.h"
#include "entities/handlers.h"

#include <algorithm>
#include <cstdarg>

void level_unsupported(level & lv, const char * fmt, ...) {
    // past a boundary nothing is traced, so nothing the sim does there can be wrong
    if (!lv.unsupported.empty() || lv.boundary) return;
    char    buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    lv.unsupported      = buf;
    lv.unsupported_line = lv.tr != nullptr ? lv.tr->lines.size() : 0;
}

void level_boundary(level & lv, const char * fmt, ...) {
    if (lv.boundary) return;
    if (trace_wants(lv.tr, nullptr)) {
        va_list args;
        va_start(args, fmt);
        trace_vadd(lv.tr, fmt, args);
        va_end(args);
    }
    lv.boundary  = true;
    lv.tr        = nullptr;
    lv.random.tr = nullptr;
}

static int64_t section_key(block_pos pos) { return section_pos_as_long(pos.x >> 4, pos.y >> 4, pos.z >> 4); }

static int32_t section_index(block_pos pos) { return (pos.y & 15) << 8 | (pos.z & 15) << 4 | (pos.x & 15); }

static bool level_outside_height(block_pos pos) { return pos.y < LEVEL_Y_MIN || pos.y > LEVEL_Y_MAX; }

bool level_in_valid_bounds(block_pos pos) {
    return !level_outside_height(pos) && pos.x >= -30000000 && pos.z >= -30000000 && pos.x < 30000000 &&
           pos.z < 30000000;
}

// the index in near_sections, -1 outside the grid
static int32_t level_near_index(block_pos pos) {
    int32_t x = (pos.x >> 4) + LEVEL_NEAR_SECTIONS / 2;
    int32_t y = (pos.y >> 4) + LEVEL_NEAR_SECTIONS / 2;
    int32_t z = (pos.z >> 4) + LEVEL_NEAR_SECTIONS / 2;
    if ((uint32_t) (x | y | z) >= LEVEL_NEAR_SECTIONS) return -1;
    return (y * LEVEL_NEAR_SECTIONS + z) * LEVEL_NEAR_SECTIONS + x;
}

static level_section * level_find_section(const level & lv, block_pos pos) {
    int32_t near = level_near_index(pos);
    if (near >= 0) return lv.near_sections[near];
    auto it = lv.sections.find(section_key(pos));
    return it == lv.sections.end() ? nullptr : it->second.get();
}

// LevelChunk.getBlockState: a section of only air reads as air, though it may still hold cave or void air
state_id level_get_state(const level & lv, block_pos pos) {
    if (level_outside_height(pos)) return block_default(BLOCK_VOID_AIR);
    level_section * s = level_find_section(lv, pos);
    return s == nullptr || s->n_non_air == 0 ? STATE_AIR : s->states[section_index(pos)];
}

// LevelChunkSection.setBlockState: returns the old state
static state_id section_set(level & lv, block_pos pos, state_id st) {
    level_section * s = level_find_section(lv, pos);
    if (s == nullptr) {
        s            = (lv.sections[section_key(pos)] = std::make_unique<level_section>()).get();
        int32_t near = level_near_index(pos);
        if (near >= 0) lv.near_sections[near] = s;
    }
    state_id & cell = s->states[section_index(pos)];
    state_id   old  = cell;
    s->n_non_air += (state_has(old, STATE_FLAG_AIR) ? 0 : -1) + (state_has(st, STATE_FLAG_AIR) ? 0 : 1);
    cell = st;
    return old;
}

std::vector<level_section_ref> level_sections(const level & lv) {
    std::vector<level_section_ref> out;
    for (const auto & [key, section] : lv.sections) {
        if (section->n_non_air > 0) out.push_back({ key, section.get() });
    }
    std::sort(out.begin(), out.end(),
              [](const level_section_ref & a, const level_section_ref & b) { return a.key < b.key; });
    return out;
}

static bool section_only_air(const level & lv, block_pos pos) {
    level_section * s = level_find_section(lv, pos);
    return s == nullptr || s->n_non_air == 0;
}

void level_set_state_raw(level & lv, block_pos pos, state_id st) {
    TMC_ASSERT(!level_outside_height(pos));
    section_set(lv, pos, st);
}

block_entity * level_new_block_entity(level & lv, block_entity_type type, block_pos pos, state_id st) {
    lv.block_entity_store.push_back(std::make_unique<block_entity>());
    block_entity * be = lv.block_entity_store.back().get();
    be->type          = type;
    be->pos           = pos;
    be->state         = st;
    return be;
}

// the block entity types the sim ports
// clang-format off
static constexpr block_entity_type BLOCK_ENTITY_TYPES_SUPPORTED[] = {
    BLOCK_ENTITY_TYPE_HOPPER, BLOCK_ENTITY_TYPE_DISPENSER, BLOCK_ENTITY_TYPE_DROPPER, BLOCK_ENTITY_TYPE_CRAFTER,
    BLOCK_ENTITY_TYPE_DECORATED_POT, BLOCK_ENTITY_TYPE_JUKEBOX, BLOCK_ENTITY_TYPE_CHISELED_BOOKSHELF,
    BLOCK_ENTITY_TYPE_SHELF, BLOCK_ENTITY_TYPE_CHEST, BLOCK_ENTITY_TYPE_TRAPPED_CHEST, BLOCK_ENTITY_TYPE_BARREL,
    BLOCK_ENTITY_TYPE_SHULKER_BOX, BLOCK_ENTITY_TYPE_COMPARATOR, BLOCK_ENTITY_TYPE_BELL,
    BLOCK_ENTITY_TYPE_ENDER_CHEST, BLOCK_ENTITY_TYPE_SKULL, BLOCK_ENTITY_TYPE_END_PORTAL, BLOCK_ENTITY_TYPE_BED,
};
// clang-format on

// EntityBlock.newBlockEntity; MovingPistonBlock makes none, its block entity comes from the piston
static block_entity * entity_block_new_block_entity(level & lv, block_pos pos, state_id st) {
    int32_t type = block_defs[state_block(st)].entity_new;
    if (type < 0) return nullptr;
    if (std::find(std::begin(BLOCK_ENTITY_TYPES_SUPPORTED), std::end(BLOCK_ENTITY_TYPES_SUPPORTED), type) ==
        std::end(BLOCK_ENTITY_TYPES_SUPPORTED)) {
        level_unsupported(lv, "block entity of %s", block_defs[state_block(st)].name);
        return nullptr;
    }
    block_entity * be = level_new_block_entity(lv, (block_entity_type) type, pos, st);
    be->items.resize(std::max(block_entity_type_container_sizes[type], 0));
    return be;
}

static void level_remove_block_entity_ticker(level & lv, block_pos pos) {
    auto it = lv.tickers_in_level.find(pos_key(pos));
    if (it != lv.tickers_in_level.end()) {
        it->second->be = nullptr;
        lv.tickers_in_level.erase(it);
    }
}

// LevelChunk.updateBlockEntityTicker: one wrapper per position keeps its place in the ticker list when rebound
static void level_update_block_entity_ticker(level & lv, block_entity * be) {
    // BlockState.getTicker
    if (!state_has(be->state, STATE_FLAG_TICKER)) {
        level_remove_block_entity_ticker(lv, be->pos);
        return;
    }
    block_entity_ticker *& ticker = lv.tickers_in_level[pos_key(be->pos)];
    if (ticker != nullptr) {
        ticker->be = be;
        return;
    }
    lv.block_entity_ticker_store.push_back(std::make_unique<block_entity_ticker>());
    ticker     = lv.block_entity_ticker_store.back().get();
    ticker->be = be;
    (lv.ticking_block_entities ? lv.pending_block_entity_tickers : lv.block_entity_tickers).push_back(ticker);
}

// LevelChunk.getBlockEntity with IMMEDIATE: makes a missing one
static block_entity * level_chunk_get_block_entity(level & lv, block_pos pos, bool create) {
    auto it = lv.block_entities.find(pos_key(pos));
    if (it == lv.block_entities.end()) {
        state_id       st = level_get_state(lv, pos);
        block_entity * be = nullptr;
        if (create && state_has(st, STATE_FLAG_BLOCK_ENTITY)) {
            be = entity_block_new_block_entity(lv, pos, st);
            if (be != nullptr) level_set_block_entity(lv, be);
        }
        return be;
    }
    return it->second;
}

block_entity * level_get_block_entity(level & lv, block_pos pos) {
    if (level_outside_height(pos)) return nullptr;
    return level_chunk_get_block_entity(lv, pos, true);
}

// Level.getBlockEntity(BlockPos, BlockEntityType): null unless the block entity has that type
block_entity * level_get_block_entity_of_type(level & lv, block_pos pos, int32_t type) {
    block_entity * be = level_get_block_entity(lv, pos);
    return be != nullptr && be->type == type ? be : nullptr;
}

// LevelChunk.setBlockEntity
static void level_chunk_set_block_entity(level & lv, block_entity * be) {
    state_id st = level_get_state(lv, be->pos);
    if (!state_has(st, STATE_FLAG_BLOCK_ENTITY)) return;
    if (st != be->state) {
        if (!block_entity_type_is_valid(be->type, st)) return;
        be->state = st;
    }
    lv.block_entities[pos_key(be->pos)] = be;
}

void level_set_block_entity(level & lv, block_entity * be) {
    if (!level_in_valid_bounds(be->pos)) return;
    // the ticker is updated even when the chunk refused the block entity
    level_chunk_set_block_entity(lv, be);
    level_update_block_entity_ticker(lv, be);
}

void level_remove_block_entity(level & lv, block_pos pos) {
    if (!level_in_valid_bounds(pos)) return;
    lv.block_entities.erase(pos_key(pos));
    level_remove_block_entity_ticker(lv, pos);
}

// LevelChunk.setBlockState: returns false where vanilla returns null (nothing changed or the block was replaced meanwhile)
static bool chunk_set_block_state(level & lv, block_pos pos, state_id st, int32_t flags, state_id & old) {
    if (section_only_air(lv, pos) && state_has(st, STATE_FLAG_AIR)) return false;
    old = section_set(lv, pos, st);
    if (old == st) return false;
    int32_t block           = state_block(st);
    bool    block_changed   = state_block(old) != block;
    bool    moved_by_piston = (flags & UPDATE_FLAG_MOVE_BY_PISTON) != 0;
    if (block_changed && state_has(old, STATE_FLAG_BLOCK_ENTITY) &&
        !block_should_changed_state_keep_block_entity(lv, st, old)) {
        if ((flags & UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS) == 0) {
            // BlockEntity.preRemoveSideEffects: containers drop their items, but shulker boxes keep them
            block_entity * be = level_get_block_entity(lv, pos);
            if (be != nullptr && be->type == BLOCK_ENTITY_TYPE_PISTON) {
                piston_moving_block_entity_final_tick(lv, be);
            } else if (be != nullptr && be->type == BLOCK_ENTITY_TYPE_JUKEBOX) {
                jukebox_block_entity_pop_out_the_item(lv, be);
            } else if (be != nullptr && block_entity_is_container(be) && be->type != BLOCK_ENTITY_TYPE_SHULKER_BOX) {
                containers_drop_contents(lv, entity_block_to_world(pos), { be, nullptr });
            }
        }
        level_remove_block_entity(lv, pos);
    }
    if ((block_changed || block_instanceof(block, VCLASS_BASE_RAIL_BLOCK)) &&
        ((flags & UPDATE_FLAG_NEIGHBORS) != 0 || moved_by_piston)) {
        block_affect_neighbors_after_removal(lv, old, pos, moved_by_piston);
    }
    if (!state_is(level_get_state(lv, pos), block)) return false;
    if ((flags & UPDATE_FLAG_SKIP_ON_PLACE) == 0) block_on_place(lv, st, pos, old, moved_by_piston);
    if (state_has(st, STATE_FLAG_BLOCK_ENTITY)) {
        block_entity * be = level_chunk_get_block_entity(lv, pos, false);
        if (be != nullptr && !block_entity_type_is_valid(be->type, st)) {
            level_remove_block_entity(lv, pos);
            be = nullptr;
        }
        if (be == nullptr) {
            be = entity_block_new_block_entity(lv, pos, st);
            if (be != nullptr) level_set_block_entity(lv, be);
        } else {
            be->state = st;
            level_update_block_entity_ticker(lv, be);
        }
    }
    return true;
}

bool level_set_block(level & lv, block_pos pos, state_id st, int32_t flags, int32_t limit) {
    if (trace_wants(lv.tr, &pos)) {
        char limit_text[32] = "";
        if (limit != UPDATE_LIMIT) snprintf(limit_text, sizeof(limit_text), " limit %d", limit);
        trace_add(lv.tr, "set %d %d %d %s flags %d%s was %s", pos.x, pos.y, pos.z, state_to_string(st).c_str(), flags,
                  limit_text, state_to_string(level_get_state(lv, pos)).c_str());
    }
    trace_enter(lv.tr);
    bool done = false;
    if (level_in_valid_bounds(pos)) {
        state_id old;
        done = chunk_set_block_state(lv, pos, st, flags, old);
        // sendBlockUpdated, light, heightmaps and POI have no effect on the simulated mechanics
        if (done && level_get_state(lv, pos) == st) {
            if ((flags & UPDATE_FLAG_NEIGHBORS) != 0) {
                level_update_neighbors_at(lv, pos, state_block(old));
                if (state_has(st, STATE_FLAG_ANALOG_OUTPUT)) {
                    level_update_neighbour_for_output_signal(lv, pos, state_block(st));
                }
            }
            if ((flags & UPDATE_FLAG_KNOWN_SHAPE) == 0 && limit > 0) {
                int32_t neighbour_flags = flags & ~(UPDATE_FLAG_NEIGHBORS | UPDATE_FLAG_SUPPRESS_DROPS);
                block_update_indirect_neighbour_shapes(lv, old, pos, neighbour_flags, limit - 1);
                block_update_neighbour_shapes(lv, st, pos, neighbour_flags, limit - 1);
                block_update_indirect_neighbour_shapes(lv, st, pos, neighbour_flags, limit - 1);
            }
        }
    }
    trace_exit(lv.tr);
    return done;
}

// FluidState.createLegacyBlock of the state at pos
static state_id level_fluid_block(level & lv, block_pos pos) {
    return fluid_state_create_legacy_block(level_get_fluid_state(lv, pos));
}

bool level_remove_block(level & lv, block_pos pos, bool moved_by_piston) {
    state_id fluid = level_fluid_block(lv, pos);
    return level_set_block(lv, pos, fluid, UPDATE_FLAG_ALL | (moved_by_piston ? UPDATE_FLAG_MOVE_BY_PISTON : 0));
}

// Mth.nextDouble
static double rng_next_double_between(rng & r, double min, double max) {
    return min >= max ? min : rng_next_double(r) * (max - min) + min;
}

// Block.popResource: a random spot around the center, half an item lower; the offsets draw even for an empty stack
void level_pop_resource(level & lv, block_pos pos, item_stack s) {
    vec3   w           = entity_block_to_world(pos);
    double half_height = entity_type_defs[ENTITY_TYPE_ITEM].height / 2.0;
    double x           = w.x + 0.5 + rng_next_double_between(lv.random, -0.25, 0.25);
    double y           = w.y + 0.5 + rng_next_double_between(lv.random, -0.25, 0.25) - half_height;
    double z           = w.z + 0.5 + rng_next_double_between(lv.random, -0.25, 0.25);
    if (!item_stack_is_empty(s)) item_entity_spawn(lv, { x, y, z }, s);
}

std::vector<item_stack> block_get_drops(level & lv, state_id st, const block_entity * be) {
    // MovingPistonBlock.getDrops: the moved block drops, or nothing without a block entity
    state_id loot = st;
    if (state_is(st, BLOCK_MOVING_PISTON)) loot = be != nullptr ? be->moved_state : STATE_AIR;
    const drop_def & drops = drop_defs[state_drops[loot]];
    if (drops.n_stacks < 0) {
        level_unsupported(lv, "random loot");
        return {};
    }
    // the shulker box item keeps the contents as a data component, which sim stacks do not carry
    if (be != nullptr && be->type == BLOCK_ENTITY_TYPE_SHULKER_BOX) {
        for (const item_stack & s : be->items) {
            if (!item_stack_is_empty(s)) level_unsupported(lv, "shulker box drop with contents");
        }
    }
    std::vector<item_stack> out;
    for (int32_t i = 0; i < drops.n_stacks; i++) {
        const drop_stack & d = drop_stacks[drops.first + i];
        out.push_back({ d.item, d.count, 0 });
    }
    return out;
}

void level_drop_resources(level & lv, state_id st, block_pos pos, const block_entity * be) {
    for (const item_stack & s : block_get_drops(lv, st, be)) level_pop_resource(lv, pos, s);
    block_spawn_after_break(lv, st, pos);
}

bool level_destroy_block(level & lv, block_pos pos, bool drop, int32_t limit) {
    state_id st = level_get_state(lv, pos);
    if (state_has(st, STATE_FLAG_AIR)) return false;
    state_id fluid = level_fluid_block(lv, pos);
    if (drop) {
        level_drop_resources(lv, st, pos,
                             state_has(st, STATE_FLAG_BLOCK_ENTITY) ? level_get_block_entity(lv, pos) : nullptr);
    }
    return level_set_block(lv, pos, fluid, UPDATE_FLAG_ALL, limit);
}

void level_update_neighbors_at(level & lv, block_pos pos, int32_t block) {
    neighbor_updater_add(lv, { .type = NEIGHBOR_UPDATE_MULTI, .pos = pos, .block = block, .skip = -1 });
}

void level_update_neighbors_around(level & lv, block_pos pos, int32_t block) {
    for (int32_t d = 0; d < 6; d++) level_update_neighbors_at(lv, pos_relative(pos, (direction) d), block);
}

void level_update_neighbors_at_except_from_facing(level & lv, block_pos pos, int32_t block, direction skip) {
    neighbor_updater_add(lv, { .type  = NEIGHBOR_UPDATE_MULTI,
                               .pos   = pos,
                               .block = block,
                               .skip  = skip,
                               .idx   = NEIGHBOR_UPDATE_ORDER[0] == skip ? 1 : 0 });
}

void level_neighbor_changed(level & lv, block_pos pos, int32_t block) {
    neighbor_updater_add(lv, { .type = NEIGHBOR_UPDATE_SIMPLE, .pos = pos, .block = block });
}

void level_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block, bool moved_by_piston) {
    neighbor_updater_add(
        lv,
        { .type = NEIGHBOR_UPDATE_FULL, .pos = pos, .state = st, .block = block, .moved_by_piston = moved_by_piston });
}

void level_neighbor_shape_changed(level & lv, direction dir, block_pos pos, block_pos neighbor_pos, state_id neighbor,
                                  int32_t flags, int32_t limit) {
    neighbor_updater_add(lv, { .type         = NEIGHBOR_UPDATE_SHAPE,
                               .pos          = pos,
                               .neighbor_pos = neighbor_pos,
                               .state        = neighbor,
                               .dir          = dir,
                               .flags        = flags,
                               .limit        = limit });
}

void level_update_neighbour_for_output_signal(level & lv, block_pos pos, int32_t block) {
    for (direction dir : DIRECTION_HORIZONTAL) {
        block_pos rel = pos_relative(pos, dir);
        state_id  st  = level_get_state(lv, rel);
        if (state_is(st, BLOCK_COMPARATOR)) {
            level_neighbor_changed(lv, st, rel, block, false);
        } else if (state_has(st, STATE_FLAG_CONDUCTOR)) {
            rel = pos_relative(rel, dir);
            st  = level_get_state(lv, rel);
            if (state_is(st, BLOCK_COMPARATOR)) level_neighbor_changed(lv, st, rel, block, false);
        }
    }
}

void level_schedule_tick(level & lv, block_pos pos, int32_t block, int32_t delay, int32_t priority) {
    // LevelAccessor.createTick takes a sub-tick number even if the tick turns out to be a duplicate
    TMC_ASSERT(delay >= 1);  // level_ticks relies on it
    scheduled_tick tick = { block, pos, lv.game_time + delay, priority, lv.sub_tick_count++ };
    if (trace_wants(lv.tr, &pos)) {
        bool dup = level_ticks_has_scheduled(lv.block_ticks, pos, block);
        trace_add(lv.tr, "sched %d %d %d %s +%d %s%s", pos.x, pos.y, pos.z, block_defs[block].name, delay,
                  tick_priority_names[priority], dup ? " dup" : "");
    }
    level_ticks_schedule(lv.block_ticks, tick);
}

void level_schedule_fluid_tick(level & lv, block_pos pos, int32_t fluid, int32_t delay) {
    TMC_ASSERT(delay >= 1);
    scheduled_tick tick = { fluid, pos, lv.game_time + delay, TICK_PRIORITY_NORMAL, lv.sub_tick_count++ };
    if (trace_wants(lv.tr, &pos)) {
        bool dup = level_ticks_has_scheduled(lv.fluid_ticks, pos, fluid);
        trace_add(lv.tr, "fsched %d %d %d %s +%d %s%s", pos.x, pos.y, pos.z, fluid_defs[fluid].name, delay,
                  tick_priority_names[TICK_PRIORITY_NORMAL], dup ? " dup" : "");
    }
    level_ticks_schedule(lv.fluid_ticks, tick);
}

static bool block_event_equals(const block_event & a, const block_event & b) {
    return a.pos == b.pos && a.block == b.block && a.param_a == b.param_a && a.param_b == b.param_b;
}

void level_block_event(level & lv, block_pos pos, int32_t block, int32_t param_a, int32_t param_b) {
    block_event e   = { pos, block, param_a, param_b };
    bool        dup = false;
    for (const block_event & other : lv.block_events) dup = dup || block_event_equals(other, e);
    if (trace_wants(lv.tr, &pos)) {
        trace_add(lv.tr, "queue %d %d %d %s %d %d%s", pos.x, pos.y, pos.z, block_defs[block].name, param_a, param_b,
                  dup ? " dup" : "");
    }
    if (!dup) lv.block_events.push_back(e);
}

int32_t level_get_direct_signal(level & lv, block_pos pos, direction dir) {
    return block_get_direct_signal(lv, level_get_state(lv, pos), pos, dir);
}

static int32_t level_get_direct_signal_to(level & lv, block_pos pos) {
    int32_t result = 0;
    for (int32_t d = 0; d < 6 && result < 15; d++) {
        direction dir = (direction) d;
        result        = std::max(result, level_get_direct_signal(lv, pos_relative(pos, dir), dir));
    }
    return result;
}

int32_t level_get_control_input_signal(level & lv, block_pos pos, direction dir, bool only_diodes) {
    state_id st = level_get_state(lv, pos);
    if (only_diodes) {
        return block_instanceof(state_block(st), VCLASS_DIODE_BLOCK) ? level_get_direct_signal(lv, pos, dir) : 0;
    }
    if (state_is(st, BLOCK_REDSTONE_BLOCK)) return 15;
    if (state_is(st, BLOCK_REDSTONE_WIRE)) return state_get(st, PROP_POWER);
    return block_is_signal_source(lv, st) ? level_get_direct_signal(lv, pos, dir) : 0;
}

bool level_has_signal(level & lv, block_pos pos, direction dir) { return level_get_signal(lv, pos, dir) > 0; }

int32_t level_get_signal(level & lv, block_pos pos, direction dir) {
    state_id st     = level_get_state(lv, pos);
    int32_t  signal = block_get_signal(lv, st, pos, dir);
    return state_has(st, STATE_FLAG_CONDUCTOR) ? std::max(signal, level_get_direct_signal_to(lv, pos)) : signal;
}

bool level_has_neighbor_signal(level & lv, block_pos pos) {
    for (int32_t d = 0; d < 6; d++) {
        if (level_has_signal(lv, pos_relative(pos, (direction) d), (direction) d)) return true;
    }
    return false;
}

int32_t level_get_best_neighbor_signal(level & lv, block_pos pos) {
    int32_t best = 0;
    for (int32_t d = 0; d < 6; d++) {
        int32_t signal = level_get_signal(lv, pos_relative(pos, (direction) d), (direction) d);
        if (signal >= 15) return 15;
        best = std::max(best, signal);
    }
    return best;
}

// ServerLevel.tickBlock
static void level_tick_block(level & lv, block_pos pos, int32_t block) {
    state_id st = level_get_state(lv, pos);
    if (trace_wants(lv.tr, &pos)) {
        trace_add(lv.tr, "tick %d %d %d %s%s", pos.x, pos.y, pos.z, block_defs[block].name,
                  state_is(st, block) ? "" : " stale");
    }
    trace_enter(lv.tr);
    if (state_is(st, block)) block_tick(lv, st, pos);
    trace_exit(lv.tr);
}

// ServerLevel.tickFluid
static void level_tick_fluid(level & lv, block_pos pos, int32_t fluid) {
    state_id       st = level_get_state(lv, pos);
    fluid_state_id fs = state_fluid_state(st);
    if (trace_wants(lv.tr, &pos)) {
        trace_add(lv.tr, "ftick %d %d %d %s%s", pos.x, pos.y, pos.z, fluid_defs[fluid].name,
                  fluid_state_get_type(fs) == fluid ? "" : " stale");
    }
    trace_enter(lv.tr);
    if (fluid_state_get_type(fs) == fluid) flowing_fluid_tick(lv, pos, st, fs);
    trace_exit(lv.tr);
}

// ServerLevel.runBlockEvents and doBlockEvent; events queued meanwhile run in the same loop
static void level_run_block_events(level & lv) {
    while (!lv.block_events.empty()) {
        block_event e = lv.block_events.front();
        lv.block_events.erase(lv.block_events.begin());
        state_id st    = level_get_state(lv, e.pos);
        bool     stale = !state_is(st, e.block);
        if (trace_wants(lv.tr, &e.pos)) {
            trace_add(lv.tr, "event %d %d %d %s %d %d%s", e.pos.x, e.pos.y, e.pos.z, block_defs[e.block].name,
                      e.param_a, e.param_b, stale ? " stale" : "");
        }
        trace_enter(lv.tr);
        if (!stale) block_trigger_event(lv, st, e.pos, e.param_a, e.param_b);
        trace_exit(lv.tr);
    }
}

// Level.tickBlockEntities; vanilla traces each ticker it calls, even when the ticker then skips its block entity
static void level_tick_block_entities(level & lv) {
    lv.ticking_block_entities = true;
    lv.block_entity_tickers.insert(lv.block_entity_tickers.end(), lv.pending_block_entity_tickers.begin(),
                                   lv.pending_block_entity_tickers.end());
    lv.pending_block_entity_tickers.clear();
    std::vector<block_entity_ticker *> & tickers = lv.block_entity_tickers;
    size_t                               kept    = 0;
    for (size_t i = 0; i < tickers.size(); i++) {
        block_entity_ticker * ticker = tickers[i];
        if (ticker->be == nullptr) continue;
        tickers[kept++]    = ticker;
        block_entity * be  = ticker->be;
        block_pos      pos = be->pos;
        if (trace_wants(lv.tr, &pos)) {
            trace_add(lv.tr, "be %d %d %d %s", pos.x, pos.y, pos.z, block_entity_type_names[be->type]);
        }
        trace_enter(lv.tr);
        // LevelChunk.BoundTickingBlockEntity.tick
        state_id st = level_get_state(lv, pos);
        if (block_entity_type_is_valid(be->type, st)) {
            if (be->type == BLOCK_ENTITY_TYPE_HOPPER) {
                hopper_block_entity_push_items_tick(lv, pos, st, be);
            } else if (be->type == BLOCK_ENTITY_TYPE_PISTON) {
                piston_moving_block_entity_tick(lv, be);
            } else if (be->type == BLOCK_ENTITY_TYPE_CRAFTER) {
                crafter_block_entity_server_tick(lv, pos, st, be);
            } else if (be->type == BLOCK_ENTITY_TYPE_JUKEBOX) {
                jukebox_block_entity_tick(lv, be);
            }
            // shulker boxes without openers and bells without entities tick without effect
        }
        trace_exit(lv.tr);
    }
    tickers.resize(kept);
    lv.ticking_block_entities = false;
}

void level_tick(level & lv) {
    trace_begin_tick(lv.tr);
    lv.handling_tick = true;
    // world border, weather, sleep, sky: off by game rules or no-ops for a void world
    lv.game_time++;

    trace_phase(lv.tr, "blockTicks");
    level_ticks_collect(lv.block_ticks, lv.game_time);
    // vanilla runs at most 65536 ticks per game tick and leaves the rest for later
    if (lv.block_ticks.to_run.size() > 65536) {
        level_unsupported(lv, "more than 65536 scheduled ticks");
        return;
    }
    scheduled_tick t;
    while (level_ticks_poll(lv.block_ticks, t)) level_tick_block(lv, t.pos, t.type);

    trace_phase(lv.tr, "fluidTicks");
    level_ticks_collect(lv.fluid_ticks, lv.game_time);
    if (lv.fluid_ticks.to_run.size() > 65536) {
        level_unsupported(lv, "more than 65536 scheduled fluid ticks");
        return;
    }
    while (level_ticks_poll(lv.fluid_ticks, t)) level_tick_fluid(lv, t.pos, t.type);
    trace_phase(lv.tr, "raid");
    trace_phase(lv.tr, "chunkSource");
    trace_phase(lv.tr, "blockEvents");
    level_run_block_events(lv);
    lv.handling_tick = false;
    trace_phase(lv.tr, "entities");
    level_tick_entities(lv);
    trace_phase(lv.tr, "blockEntities");
    level_tick_block_entities(lv);
    trace_phase(lv.tr, "entityManagement");
    trace_phase(lv.tr, "between");
}
