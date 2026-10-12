// Containers: the Container interface, and the chests, jukeboxes, bookshelves, shelves and composters built on it

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"
#include "entities/handlers.h"

#include <algorithm>
#include <cmath>

static void       jukebox_block_entity_set_the_item(level & lv, block_entity * be, item_stack s);
static item_stack chiseled_book_shelf_block_entity_remove_item(level & lv, block_entity * be, int32_t slot);
static void       chiseled_book_shelf_block_entity_set_item(level & lv, block_entity * be, int32_t slot, item_stack s);
static int32_t    composter_container_get_container_size(const composter_container & c);
static void       composter_container_set_changed(level & lv, composter_container & c);

// Container: block entities (HopperBlockEntity overrides, CompoundContainer) and SimpleContainer for the composter.
// ContainerEntity keeps the defaults, and its setChanged does nothing.

// BlockEntity.setChanged
void block_entity_set_changed(level & lv, block_pos pos, state_id st) {
    if (!state_has(st, STATE_FLAG_AIR)) level_update_neighbour_for_output_signal(lv, pos, state_block(st));
}

bool block_entity_is_container(const block_entity * be) { return block_entity_type_container_sizes[be->type] >= 0; }

int32_t container_get_container_size(const container & c) {
    if (c.composter != nullptr) return composter_container_get_container_size(*c.composter);
    if (c.vehicle != nullptr) return (int32_t) c.vehicle->items.size();
    return (int32_t) c.first->items.size() + (c.second != nullptr ? (int32_t) c.second->items.size() : 0);
}

// CompoundContainer
static block_entity * container_half(const container & c, int32_t & slot) {
    int32_t first_size = (int32_t) c.first->items.size();
    if (c.second != nullptr && slot >= first_size) {
        slot -= first_size;
        return c.second;
    }
    return c.first;
}

item_stack & container_get_item(const container & c, int32_t slot) {
    if (c.composter != nullptr) return c.composter->item;
    if (c.vehicle != nullptr) return c.vehicle->items[slot];
    block_entity * be = container_half(c, slot);
    return be->items[slot];
}

bool container_is_empty(const container & c) {
    for (int32_t slot = 0; slot < container_get_container_size(c); slot++) {
        if (!item_stack_is_empty(container_get_item(c, slot))) return false;
    }
    return true;
}

// hoppers and chests keep Container's default limit of 99
int32_t container_get_max_stack_size(const item_stack & s) { return std::min(99, item_stack_get_max_stack_size(s)); }

// Container.getMaxStackSize(ItemStack): the composter's containers hold one item
int32_t container_get_max_stack_size_in(const container & c, const item_stack & s) {
    int32_t limit = c.composter != nullptr ? 1 :
                    c.vehicle != nullptr   ? 99 :
                                             block_entity_type_max_stack_sizes[c.first->type];
    return std::min(limit, item_stack_get_max_stack_size(s));
}

// HopperBlockEntity and ContainerSingleItem (the decorated pot) override removeItem and setItem without setChanged
item_stack container_remove_item(level & lv, const container & c, int32_t slot, int32_t count) {
    if (c.composter != nullptr) {
        item_stack & stack  = c.composter->item;
        item_stack   result = !item_stack_is_empty(stack) && count > 0 ? item_stack_split(stack, count) : item_stack();
        if (!item_stack_is_empty(result)) composter_container_set_changed(lv, *c.composter);
        return result;
    }
    if (c.vehicle != nullptr) {
        item_stack & stack = c.vehicle->items[slot];
        return !item_stack_is_empty(stack) && count > 0 ? item_stack_split(stack, count) : item_stack();
    }
    block_entity * be = container_half(c, slot);
    // JukeboxBlockEntity.splitTheItem hands out the whole record
    if (be->type == BLOCK_ENTITY_TYPE_JUKEBOX) {
        item_stack record = be->items[slot];
        jukebox_block_entity_set_the_item(lv, be, item_stack());
        return record;
    }
    if (be->type == BLOCK_ENTITY_TYPE_CHISELED_BOOKSHELF) {
        return chiseled_book_shelf_block_entity_remove_item(lv, be, slot);
    }
    item_stack & stack  = be->items[slot];
    item_stack   result = !item_stack_is_empty(stack) && count > 0 ? item_stack_split(stack, count) : item_stack();
    bool         quiet  = be->type == BLOCK_ENTITY_TYPE_HOPPER || be->type == BLOCK_ENTITY_TYPE_DECORATED_POT;
    if (!item_stack_is_empty(result)) {
        block_entity_items_changed(lv, be);
        if (!quiet) block_entity_set_changed(lv, be->pos, be->state);
    }
    return result;
}

void container_set_item(level & lv, const container & c, int32_t slot, item_stack s) {
    if (c.composter != nullptr) {
        c.composter->item = s;
        item_stack_limit_size(c.composter->item, 1);
        composter_container_set_changed(lv, *c.composter);
        return;
    }
    if (c.vehicle != nullptr) {
        c.vehicle->items[slot] = s;
        item_stack_limit_size(c.vehicle->items[slot], container_get_max_stack_size(s));
        return;
    }
    block_entity * be = container_half(c, slot);
    if (be->type == BLOCK_ENTITY_TYPE_JUKEBOX) {
        jukebox_block_entity_set_the_item(lv, be, s);
        return;
    }
    if (be->type == BLOCK_ENTITY_TYPE_CHISELED_BOOKSHELF) {
        chiseled_book_shelf_block_entity_set_item(lv, be, slot, s);
        return;
    }
    be->items[slot] = s;
    block_entity_items_changed(lv, be);
    // ContainerSingleItem.setItem does not limit the stack either
    if (be->type == BLOCK_ENTITY_TYPE_DECORATED_POT) return;
    item_stack_limit_size(be->items[slot], container_get_max_stack_size(s));
    if (be->type != BLOCK_ENTITY_TYPE_HOPPER) block_entity_set_changed(lv, be->pos, be->state);
}

void container_set_changed(level & lv, const container & c) {
    if (c.vehicle != nullptr) return;
    if (c.composter != nullptr) {
        composter_container_set_changed(lv, *c.composter);
        return;
    }
    block_entity_set_changed(lv, c.first->pos, c.first->state);
    if (c.second != nullptr) block_entity_set_changed(lv, c.second->pos, c.second->state);
}

void container_items_changed(level & lv, const container & c, int32_t slot) {
    if (c.composter == nullptr && c.vehicle == nullptr) block_entity_items_changed(lv, container_half(c, slot));
}

// AbstractContainerMenu.getRedstoneSignalFromContainer; the first block entity caches it until either half's items change
int32_t container_get_redstone_signal(const container * c) {
    if (c == nullptr) return 0;
    TMC_ASSERT(c->composter == nullptr);
    block_entity * be = c->first;
    if (be != nullptr && be->signal >= 0 && be->signal_mod_count == be->mod_count && be->signal_second == c->second &&
        (c->second == nullptr || be->signal_second_mod_count == c->second->mod_count)) {
        return be->signal;
    }
    float total_percent = 0.0f;
    for (int32_t slot = 0; slot < container_get_container_size(*c); slot++) {
        const item_stack & s = container_get_item(*c, slot);
        if (!item_stack_is_empty(s)) total_percent += (float) s.count / (float) container_get_max_stack_size(s);
    }
    total_percent /= (float) container_get_container_size(*c);
    // Mth.lerpDiscrete from 0 to 15
    int32_t signal = (int32_t) std::floor(total_percent * 14.0f) + (total_percent > 0.0f ? 1 : 0);
    if (be != nullptr) {
        be->signal                  = signal;
        be->signal_mod_count        = be->mod_count;
        be->signal_second           = c->second;
        be->signal_second_mod_count = c->second != nullptr ? c->second->mod_count : 0;
    }
    return signal;
}

// AbstractContainerMenu.getRedstoneSignalFromBlockEntity, the getAnalogOutputSignal of most containers
int32_t container_get_redstone_signal_from_block_entity(level & lv, state_id, block_pos pos, direction) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || !block_entity_is_container(be)) return 0;
    container c = { be, nullptr };
    return container_get_redstone_signal(&c);
}

// Containers.dropItemStack: one random spot in the block, even for an empty stack, then the stack in random portions
static void containers_drop_item_stack(level & lv, const vec3 & at, item_stack & s) {
    double xo = std::floor(at.x) + rng_next_double(lv.random) * 0.75 + 0.125;
    double yo = std::floor(at.y) + rng_next_double(lv.random) * 0.75;
    double zo = std::floor(at.z) + rng_next_double(lv.random) * 0.75 + 0.125;
    while (!item_stack_is_empty(s)) {
        item_stack part = item_stack_split(s, rng_next_int(lv.random, 21) + 10);
        entity *   e    = item_entity_new_random_motion(lv, { xo, yo, zo }, part);
        double     dx   = rng_triangle(lv.random, 0.0, 0.11485000171139836);
        double     dy   = rng_triangle(lv.random, 0.2, 0.11485000171139836);
        double     dz   = rng_triangle(lv.random, 0.0, 0.11485000171139836);
        e->delta        = { dx, dy, dz };
        level_add_fresh_entity(lv, e);
    }
}

// Containers.dropContents; the slots empty as they drop
void containers_drop_contents(level & lv, const vec3 & at, const container & c) {
    for (int32_t slot = 0; slot < container_get_container_size(c); slot++) {
        containers_drop_item_stack(lv, at, container_get_item(c, slot));
    }
}

// Containers.updateNeighboursAfterDestroy, the affectNeighborsAfterRemoval of most containers
void containers_update_neighbours_after_destroy(level & lv, state_id st, block_pos pos, bool) {
    level_update_neighbour_for_output_signal(lv, pos, state_block(st));
}

// BlockEntity.triggerEvent: event 1 moves chest, ender chest and shulker box lids, swings bells and wobbles decorated pots
bool base_entity_block_trigger_event(level & lv, state_id, block_pos pos, int32_t param_a, int32_t param_b) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || param_a != 1) return false;
    switch (be->type) {
        case BLOCK_ENTITY_TYPE_CHEST:
        case BLOCK_ENTITY_TYPE_TRAPPED_CHEST:
        case BLOCK_ENTITY_TYPE_ENDER_CHEST:
        case BLOCK_ENTITY_TYPE_SHULKER_BOX:
        case BLOCK_ENTITY_TYPE_BELL: return true;
        case BLOCK_ENTITY_TYPE_DECORATED_POT:
            // only the two WobbleStyle ordinals
            return param_b >= 0 && param_b < 2;
        default: return false;
    }
}

// ChestBlock

static chest_type chest_get_type(state_id st) { return (chest_type) state_get(st, PROP_CHEST_TYPE); }

direction chest_get_connected_direction(state_id st) {
    direction facing = state_facing(st);
    return chest_get_type(st) == CHEST_TYPE_LEFT ? direction_clockwise(facing) : direction_counter_clockwise(facing);
}

// ChestBlock.isChestBlockedAt; cats are entities, which the sim does not have
static bool chest_is_blocked_at(level & lv, block_pos pos) {
    return state_has(level_get_state(lv, pos_relative(pos, DIRECTION_UP)), STATE_FLAG_CONDUCTOR);
}

// BlockEntityType.getBlockEntity
static block_entity * chest_get_typed_block_entity(level & lv, state_id st, block_pos pos) {
    return level_get_block_entity_of_type(lv, pos, block_defs[state_block(st)].entity_new);
}

// ChestBlock.combine (DoubleBlockCombiner.combineWithNeigbour) with CHEST_COMBINER; false is acceptNone
bool chest_get_container(level & lv, state_id st, block_pos pos, bool ignore_being_blocked, container & out) {
    block_entity * be = chest_get_typed_block_entity(lv, st, pos);
    if (be == nullptr || (!ignore_being_blocked && chest_is_blocked_at(lv, pos))) return false;
    out             = { be, nullptr };
    chest_type type = chest_get_type(st);
    if (type == CHEST_TYPE_SINGLE) return true;
    block_pos neighbor_pos = pos_relative(pos, chest_get_connected_direction(st));
    state_id  neighbor     = level_get_state(lv, neighbor_pos);
    if (state_is(neighbor, state_block(st)) && chest_get_type(neighbor) != CHEST_TYPE_SINGLE &&
        chest_get_type(neighbor) != type && state_facing(neighbor) == state_facing(st)) {
        if (!ignore_being_blocked && chest_is_blocked_at(lv, neighbor_pos)) return false;
        block_entity * neighbor_be = chest_get_typed_block_entity(lv, neighbor, neighbor_pos);
        if (neighbor_be != nullptr) {
            // the RIGHT half comes first
            out = type == CHEST_TYPE_RIGHT ? container{ be, neighbor_be } : container{ neighbor_be, be };
        }
    }
    return true;
}

// virtual: copper chests connect to any copper chest
static bool chest_can_connect_to(state_id st, state_id neighbor) {
    if (block_instanceof(state_block(st), VCLASS_COPPER_CHEST_BLOCK)) {
        return state_has(neighbor, STATE_FLAG_TAG_COPPER_CHESTS) && state_has_prop(neighbor, PROP_CHEST_TYPE);
    }
    return state_is(neighbor, state_block(st));
}

static state_id chest_update_shape(level &, state_id st, block_pos, direction dir, block_pos, state_id neighbor) {
    if (chest_can_connect_to(st, neighbor) && direction_is_horizontal(dir)) {
        chest_type neighbor_type = chest_get_type(neighbor);
        if (chest_get_type(st) == CHEST_TYPE_SINGLE && neighbor_type != CHEST_TYPE_SINGLE &&
            state_facing(st) == state_facing(neighbor) &&
            chest_get_connected_direction(neighbor) == direction_opposite(dir)) {
            // ChestType.getOpposite swaps LEFT and RIGHT
            return state_set(st, PROP_CHEST_TYPE,
                             neighbor_type == CHEST_TYPE_LEFT ? CHEST_TYPE_RIGHT : CHEST_TYPE_LEFT);
        }
    } else if (chest_get_connected_direction(st) == dir) {
        return state_set(st, PROP_CHEST_TYPE, CHEST_TYPE_SINGLE);
    }
    return st;
}

// CopperChestBlock.updateShape: a double copper chest takes on the block of its other half
static state_id copper_chest_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos neighbor_pos,
                                          state_id neighbor) {
    state_id result = chest_update_shape(lv, st, pos, dir, neighbor_pos, neighbor);
    if (chest_can_connect_to(st, neighbor) && chest_get_type(result) != CHEST_TYPE_SINGLE &&
        chest_get_connected_direction(result) == dir) {
        return block_with_properties_of(state_block(neighbor), result);
    }
    return result;
}

static int32_t chest_get_analog_output_signal(level & lv, state_id st, block_pos pos, direction) {
    container c;
    bool      found = chest_get_container(lv, st, pos, false, c);
    return container_get_redstone_signal(found ? &c : nullptr);
}

// CopperChestBlock.shouldChangedStateKeepBlockEntity: a copper chest that oxidizes or gets waxed keeps its items
static bool copper_chest_should_changed_state_keep_block_entity(state_id, state_id old) {
    return state_has(old, STATE_FLAG_TAG_COPPER_CHESTS);
}

// ChestBlockEntity.getOpenCount. Openers are players, and there are none: the count stays 0.
static int32_t chest_block_entity_get_open_count(level & lv, state_id st, block_pos pos) {
    if (state_has(st, STATE_FLAG_BLOCK_ENTITY)) level_get_block_entity(lv, pos);
    return 0;
}

// TrappedChestBlock

static int32_t trapped_chest_get_signal(level & lv, state_id st, block_pos pos, direction) {
    return std::clamp(chest_block_entity_get_open_count(lv, st, pos), 0, 15);
}

static int32_t trapped_chest_get_direct_signal(level & lv, state_id st, block_pos pos, direction dir) {
    return dir == DIRECTION_UP ? trapped_chest_get_signal(lv, st, pos, dir) : 0;
}

// JukeboxBlock, JukeboxBlockEntity and JukeboxSongPlayer. The song is always the record's JukeboxSong while one plays.

static void jukebox_block_entity_on_song_changed(level & lv, block_entity * be) {
    level_update_neighbors_at(lv, be->pos, state_block(be->state));
    block_entity_set_changed(lv, be->pos, be->state);
}

static void jukebox_song_player_stop(level & lv, block_entity * be) {
    if (be->song_playing) {
        be->song_playing             = false;
        be->ticks_since_song_started = 0;
        jukebox_block_entity_on_song_changed(lv, be);
    }
}

// JukeboxSong.hasFinished: the song runs 20 ticks past its length
bool jukebox_song_has_finished(int32_t record, int64_t ticks) { return ticks >= item_defs[record].song_ticks + 20; }

// JukeboxBlockEntity.setTheItem; notifyItemChangedInJukebox only acts while the level holds the cached state
static void jukebox_block_entity_set_the_item(level & lv, block_entity * be, item_stack s) {
    be->items[0]  = s;
    block_entity_items_changed(lv, be);
    bool inserted = !item_stack_is_empty(s);
    if (level_get_state(lv, be->pos) == be->state) {
        level_set_block(lv, be->pos, state_set(be->state, PROP_HAS_RECORD, inserted), UPDATE_FLAG_CLIENTS);
    }
    if (inserted && item_defs[s.item].song_ticks >= 0) {
        // JukeboxSongPlayer.play
        be->song_playing             = true;
        be->ticks_since_song_started = 0;
        jukebox_block_entity_on_song_changed(lv, be);
    } else {
        jukebox_song_player_stop(lv, be);
    }
}

// JukeboxBlockEntity.popOutTheItem
void jukebox_block_entity_pop_out_the_item(level & lv, block_entity * be) {
    item_stack s = be->items[0];
    if (item_stack_is_empty(s)) return;
    jukebox_block_entity_set_the_item(lv, be, item_stack());
    // Vec3.offsetRandomXZ by 0.7
    vec3     w      = entity_block_to_world(be->pos);
    float    ox     = (rng_next_float(lv.random) - 0.5F) * 0.7F;
    float    oz     = (rng_next_float(lv.random) - 0.5F) * 0.7F;
    entity * e      = item_entity_new_random_motion(lv, { w.x + 0.5 + ox, w.y + 1.01, w.z + 0.5 + oz }, s);
    e->pickup_delay = 10;
    level_add_fresh_entity(lv, e);
    jukebox_block_entity_on_song_changed(lv, be);
}

// JukeboxSongPlayer.tick: the note particle color comes from the level random every 20 ticks
void jukebox_block_entity_tick(level & lv, block_entity * be) {
    if (!be->song_playing) return;
    if (jukebox_song_has_finished(be->items[0].item, be->ticks_since_song_started)) {
        jukebox_song_player_stop(lv, be);
        return;
    }
    if (be->ticks_since_song_started % 20 == 0) rng_next_int(lv.random, 4);
    be->ticks_since_song_started++;
}

static int32_t jukebox_get_signal(level & lv, state_id, block_pos pos, direction) {
    block_entity * be = level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_JUKEBOX);
    return be != nullptr && be->song_playing ? 15 : 0;
}

static int32_t jukebox_get_analog_output_signal(level & lv, state_id, block_pos pos, direction) {
    block_entity * be = level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_JUKEBOX);
    return be != nullptr ? item_defs[be->items[0].item].song_comparator : 0;
}

// ChiseledBookShelfBlock and ChiseledBookShelfBlockEntity. The slot properties are kept apart from the items, so a slot can show a book it does not hold.

static prop_id chiseled_book_shelf_slot_occupied(int32_t slot) { return (prop_id) (PROP_SLOT_0_OCCUPIED + slot); }

// ChiseledBookShelfBlockEntity.updateState starts from the block entity's cached state
static void chiseled_book_shelf_block_entity_update_state(level & lv, block_entity * be, int32_t interacted_slot) {
    be->last_interacted_slot = interacted_slot;
    state_id updated         = be->state;
    for (int32_t slot = 0; slot < 6; slot++) {
        updated = state_set(updated, chiseled_book_shelf_slot_occupied(slot), !item_stack_is_empty(be->items[slot]));
    }
    level_set_block(lv, be->pos, updated, UPDATE_FLAG_ALL);
}

// ChiseledBookShelfBlockEntity.removeItem takes the whole slot and skips setChanged
static item_stack chiseled_book_shelf_block_entity_remove_item(level & lv, block_entity * be, int32_t slot) {
    item_stack retrieved = be->items[slot];
    be->items[slot]      = item_stack();
    block_entity_items_changed(lv, be);
    if (!item_stack_is_empty(retrieved)) chiseled_book_shelf_block_entity_update_state(lv, be, slot);
    return retrieved;
}

// ChiseledBookShelfBlockEntity.setItem drops items outside BOOKSHELF_BOOKS without a word
static void chiseled_book_shelf_block_entity_set_item(level & lv, block_entity * be, int32_t slot, item_stack s) {
    if (item_defs[item_stack_get_item(s)].tags & ITEM_TAG_BOOKSHELF_BOOKS) {
        be->items[slot] = s;
        block_entity_items_changed(lv, be);
        chiseled_book_shelf_block_entity_update_state(lv, be, slot);
    } else if (item_stack_is_empty(s)) {
        chiseled_book_shelf_block_entity_remove_item(lv, be, slot);
    }
}

// ListBackedContainer.canPlaceItem with getMaxStackSize 1
bool chiseled_book_shelf_block_entity_can_place_item(const block_entity * be, int32_t slot, const item_stack & s) {
    return (item_defs[item_stack_get_item(s)].tags & ITEM_TAG_BOOKSHELF_BOOKS) && item_stack_is_empty(be->items[slot]);
}

// ChiseledBookShelfBlockEntity.canTakeItem
bool chiseled_book_shelf_block_entity_can_take_item(const container & into, const item_stack & s) {
    for (int32_t slot = 0; slot < container_get_container_size(into); slot++) {
        const item_stack & to = container_get_item(into, slot);
        if (item_stack_is_empty(to) ||
            (item_stack_is_same_item_same_components(s, to) &&
             item_stack_get_count(to) + item_stack_get_count(s) <= container_get_max_stack_size(to))) {
            return true;
        }
    }
    return false;
}

static int32_t chiseled_book_shelf_get_analog_output_signal(level & lv, state_id, block_pos pos, direction) {
    block_entity * be = level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_CHISELED_BOOKSHELF);
    return be != nullptr ? be->last_interacted_slot + 1 : 0;
}

// ShelfBlock, ShelfBlockEntity and SideChainPartBlock: powered wooden shelves facing the same way chain up to 3 wide.

static constexpr int32_t SHELF_MAX_CHAIN_LENGTH = 3;

static bool shelf_is_connectable(state_id st) {
    return state_has(st, STATE_FLAG_TAG_WOODEN_SHELVES) && state_get(st, PROP_POWERED);
}

// SideChainPartBlock.Neighbors.createNewNeighbor: the neighbor's part, or -1 for an EmptyNeighbor
static int32_t shelf_neighbor_part(level & lv, block_pos pos, direction facing) {
    state_id st = level_get_state(lv, pos);
    return shelf_is_connectable(st) && state_facing(st) == facing ? state_get(st, PROP_SIDE_CHAIN_PART) : -1;
}

// Neighbors.left and right: left is clockwise of the facing
static block_pos shelf_neighbor_pos(block_pos center, direction facing, bool left, int32_t steps) {
    return pos_relative_steps(center, left ? direction_clockwise(facing) : direction_counter_clockwise(facing), steps);
}

// SideChainPartBlock.getAllBlocksConnectedTo, left end first
static std::vector<block_pos> shelf_get_all_blocks_connected_to(level & lv, block_pos pos) {
    state_id st = level_get_state(lv, pos);
    if (!shelf_is_connectable(st)) return {};
    direction              facing  = state_facing(st);
    std::vector<block_pos> results = { pos };
    for (int32_t side = 0; side < 2; side++) {
        bool left = side == 0;
        for (int32_t steps = 1; steps < SHELF_MAX_CHAIN_LENGTH; steps++) {
            block_pos neighbor = shelf_neighbor_pos(pos, facing, left, steps);
            int32_t   part     = shelf_neighbor_part(lv, neighbor, facing);
            int32_t   end_part = left ? SIDE_CHAIN_PART_LEFT : SIDE_CHAIN_PART_RIGHT;
            if (part == SIDE_CHAIN_PART_CENTER || (part >= 0 && part == end_part)) {
                results.insert(left ? results.begin() : results.end(), neighbor);
            }
            if (part != SIDE_CHAIN_PART_CENTER) break;
        }
    }
    return results;
}

// SideChainPartBlock.setPart reads the live state, while callers pass a part from a neighbor read earlier
static void shelf_set_part(level & lv, block_pos pos, int32_t part) {
    state_id st = level_get_state(lv, pos);
    if (state_get(st, PROP_SIDE_CHAIN_PART) != part) {
        level_set_block(lv, pos, state_set(st, PROP_SIDE_CHAIN_PART, part), UPDATE_FLAG_ALL);
    }
}

// SideChainPartBlock.updateNeighborsAfterPoweringDown: the right neighbor is read after the left one changed
static void shelf_update_neighbors_after_powering_down(level & lv, block_pos pos, state_id st) {
    direction facing = state_facing(st);
    block_pos left   = shelf_neighbor_pos(pos, facing, true, 1);
    int32_t   part   = shelf_neighbor_part(lv, left, facing);
    if (part >= 0) shelf_set_part(lv, left, side_chain_part_when_disconnected_from_the_right[part]);
    block_pos right = shelf_neighbor_pos(pos, facing, false, 1);
    part            = shelf_neighbor_part(lv, right, facing);
    if (part >= 0) shelf_set_part(lv, right, side_chain_part_when_disconnected_from_the_left[part]);
}

static bool shelf_can_connect(int32_t new_blocks_to_connect_to, int32_t current_chain_length) {
    return new_blocks_to_connect_to > 0 && current_chain_length + new_blocks_to_connect_to <= SHELF_MAX_CHAIN_LENGTH;
}

// SideChainPartBlock.updateSelfAndNeighborsOnPoweringUp: both neighbor parts are read before either changes
static void shelf_update_self_and_neighbors_on_powering_up(level & lv, block_pos pos, state_id st, state_id old) {
    if (!shelf_is_connectable(st)) return;
    // isBeingUpdatedByNeighbor
    if (state_get(st, PROP_SIDE_CHAIN_PART) != SIDE_CHAIN_PART_UNCONNECTED ||
        (shelf_is_connectable(old) && state_get(old, PROP_SIDE_CHAIN_PART) != SIDE_CHAIN_PART_UNCONNECTED)) {
        return;
    }
    direction facing        = state_facing(st);
    block_pos left          = shelf_neighbor_pos(pos, facing, true, 1);
    int32_t   left_part     = shelf_neighbor_part(lv, left, facing);
    int32_t   chain_left    = left_part >= 0 ? (int32_t) shelf_get_all_blocks_connected_to(lv, left).size() : 0;
    block_pos right         = shelf_neighbor_pos(pos, facing, false, 1);
    int32_t   right_part    = shelf_neighbor_part(lv, right, facing);
    int32_t   chain_right   = right_part >= 0 ? (int32_t) shelf_get_all_blocks_connected_to(lv, right).size() : 0;
    int32_t   self_part     = SIDE_CHAIN_PART_UNCONNECTED;
    int32_t   current_chain = 1;
    if (shelf_can_connect(chain_left, current_chain)) {
        self_part = side_chain_part_when_connected_to_the_left[self_part];
        shelf_set_part(lv, left, side_chain_part_when_connected_to_the_right[left_part]);
        current_chain += chain_left;
    }
    if (shelf_can_connect(chain_right, current_chain)) {
        self_part = side_chain_part_when_connected_to_the_right[self_part];
        shelf_set_part(lv, right, side_chain_part_when_connected_to_the_left[right_part]);
    }
    shelf_set_part(lv, pos, self_part);
}

static void shelf_on_place(level & lv, state_id st, block_pos pos, state_id old, bool) {
    if (state_get(st, PROP_POWERED)) {
        shelf_update_self_and_neighbors_on_powering_up(lv, pos, st, old);
    } else {
        shelf_update_neighbors_after_powering_down(lv, pos, st);
    }
}

static void shelf_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (state_get(st, PROP_POWERED) == signal) return;
    state_id new_state = state_set(st, PROP_POWERED, signal);
    if (!signal) new_state = state_set(new_state, PROP_SIDE_CHAIN_PART, SIDE_CHAIN_PART_UNCONNECTED);
    level_set_block(lv, pos, new_state, UPDATE_FLAG_ALL);
}

static void shelf_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    containers_update_neighbours_after_destroy(lv, st, pos, moved_by_piston);
    shelf_update_neighbors_after_powering_down(lv, pos, st);
}

// ShelfBlock.getAnalogOutputSignal
static int32_t shelf_get_analog_output_signal(level & lv, state_id st, block_pos pos, direction dir) {
    block_entity * be = level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_SHELF);
    if (dir != direction_opposite(state_facing(st)) || be == nullptr) return 0;
    int32_t bits = 0;
    for (int32_t slot = 0; slot < 3; slot++) bits |= (item_stack_is_empty(be->items[slot]) ? 0 : 1) << slot;
    return bits;
}

// ComposterBlock

// the first item into an empty composter always counts; any other draws a double against its chance
static void composter_add_item(level & lv, state_id st, block_pos pos, const item_stack & stack) {
    int32_t level_value = state_get(st, PROP_LEVEL_COMPOSTER);
    float   chance      = item_defs[stack.item].compost_chance;
    if ((level_value != 0 || !(chance > 0.0f)) && !(rng_next_double(lv.random) < (double) chance)) return;
    level_set_block(lv, pos, state_set(st, PROP_LEVEL_COMPOSTER, level_value + 1), UPDATE_FLAG_ALL);
    if (level_value + 1 == 7) level_schedule_tick(lv, pos, state_block(st), 20);
}

static void composter_on_place(level & lv, state_id st, block_pos pos, state_id, bool) {
    if (state_get(st, PROP_LEVEL_COMPOSTER) == 7) level_schedule_tick(lv, pos, state_block(st), 20);
}

static void composter_tick(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_LEVEL_COMPOSTER) == 7) {
        level_set_block(lv, pos, state_set(st, PROP_LEVEL_COMPOSTER, 8), UPDATE_FLAG_ALL);
    }
}

static int32_t composter_get_analog_output_signal(level &, state_id st, block_pos, direction) {
    return state_get(st, PROP_LEVEL_COMPOSTER);
}

// ComposterBlock.getContainer
void composter_get_container(state_id st, block_pos pos, composter_container & out) {
    out = {
        st, pos, false, state_get(st, PROP_LEVEL_COMPOSTER) == 8 ? item_stack{ ITEM_BONE_MEAL, 1 }
                : item_stack()
    };
}

static int32_t composter_container_get_container_size(const composter_container & c) {
    return state_get(c.state, PROP_LEVEL_COMPOSTER) == 7 ? 0 : 1;
}

int32_t composter_container_get_slots_for_face(const composter_container & c, direction face) {
    int32_t level_value = state_get(c.state, PROP_LEVEL_COMPOSTER);
    return (level_value < 7 && face == DIRECTION_UP) || (level_value == 8 && face == DIRECTION_DOWN) ? 1 : 0;
}

// one item per container: the first one composts and sets changed
bool composter_container_can_place_item_through_face(const composter_container & c, const item_stack & s,
                                                     const direction * face) {
    return state_get(c.state, PROP_LEVEL_COMPOSTER) < 7 && !c.changed && face != nullptr && *face == DIRECTION_UP &&
           item_defs[s.item].compost_chance >= 0.0f;
}

bool composter_container_can_take_item_through_face(const composter_container & c, const item_stack & s,
                                                    direction face) {
    return state_get(c.state, PROP_LEVEL_COMPOSTER) == 8 && !c.changed && face == DIRECTION_DOWN &&
           s.item == ITEM_BONE_MEAL;
}

// the input composts what it holds; the output empties the composter each time, from the state it was made with
static void composter_container_set_changed(level & lv, composter_container & c) {
    int32_t level_value = state_get(c.state, PROP_LEVEL_COMPOSTER);
    if (level_value == 8) {
        level_set_block(lv, c.pos, state_set(c.state, PROP_LEVEL_COMPOSTER, 0), UPDATE_FLAG_ALL);
        c.changed = true;
    } else if (level_value < 7 && !item_stack_is_empty(c.item)) {
        c.changed = true;
        composter_add_item(lv, c.state, c.pos, c.item);
        c.item = item_stack();
    }
}

// DecoratedPotBlock.onProjectileHit
static void decorated_pot_on_projectile_hit(level & lv, state_id st, const hit_result & hit, entity & projectile) {
    if (!(entity_type_defs[projectile.type].tags & ENTITY_TYPE_TAG_IMPACT_PROJECTILES)) return;
    level_set_block(lv, hit.pos, state_set(st, PROP_CRACKED, 1),
                    UPDATE_FLAG_INVISIBLE | UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
    level_destroy_block(lv, hit.pos, true);
}

// clang-format off
const std::vector<block_hooks> container_hooks = {
    { .vclass = VCLASS_BASE_ENTITY_BLOCK, .trigger_event = base_entity_block_trigger_event },
    { .vclass = VCLASS_CHEST_BLOCK,
      .update_shape = chest_update_shape,
      .affect_neighbors_after_removal = containers_update_neighbours_after_destroy,
      .get_analog_output_signal = chest_get_analog_output_signal },
    { .vclass = VCLASS_COPPER_CHEST_BLOCK,
      .update_shape = copper_chest_update_shape,
      .should_changed_state_keep_block_entity = copper_chest_should_changed_state_keep_block_entity },
    { .vclass = VCLASS_TRAPPED_CHEST_BLOCK,
      .get_signal = trapped_chest_get_signal,
      .get_direct_signal = trapped_chest_get_direct_signal },
    { .vclass = VCLASS_ENDER_CHEST_BLOCK, .update_shape = block_behaviour_update_shape },
    { .vclass = VCLASS_BARREL_BLOCK,
      .affect_neighbors_after_removal = containers_update_neighbours_after_destroy,
      .get_analog_output_signal = container_get_redstone_signal_from_block_entity },
    { .vclass = VCLASS_SHULKER_BOX_BLOCK,
      .affect_neighbors_after_removal = containers_update_neighbours_after_destroy,
      .get_analog_output_signal = container_get_redstone_signal_from_block_entity },
    { .vclass = VCLASS_DECORATED_POT_BLOCK,
      .update_shape = block_behaviour_update_shape,
      .affect_neighbors_after_removal = containers_update_neighbours_after_destroy,
      .get_analog_output_signal = container_get_redstone_signal_from_block_entity,
      .on_projectile_hit = decorated_pot_on_projectile_hit },
    { .vclass = VCLASS_JUKEBOX_BLOCK,
      .affect_neighbors_after_removal = containers_update_neighbours_after_destroy,
      .get_signal = jukebox_get_signal,
      .get_analog_output_signal = jukebox_get_analog_output_signal },
    { .vclass = VCLASS_CHISELED_BOOK_SHELF_BLOCK,
      .affect_neighbors_after_removal = containers_update_neighbours_after_destroy,
      .get_analog_output_signal = chiseled_book_shelf_get_analog_output_signal },
    { .vclass = VCLASS_SHELF_BLOCK,
      .update_shape = block_behaviour_update_shape,
      .neighbor_changed = shelf_neighbor_changed,
      .on_place = shelf_on_place,
      .affect_neighbors_after_removal = shelf_affect_neighbors_after_removal,
      .get_analog_output_signal = shelf_get_analog_output_signal },
    { .vclass = VCLASS_COMPOSTER_BLOCK,
      .on_place = composter_on_place,
      .tick = composter_tick,
      .get_analog_output_signal = composter_get_analog_output_signal },
};
// clang-format on
