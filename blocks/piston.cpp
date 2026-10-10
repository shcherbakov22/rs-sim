// Pistons: the base, its head, the structure it pushes and the moving blocks in between

#include "blocks/handlers.h"
#include "engine/java-hash-map.h"
#include "engine/level.h"

#include <algorithm>

// PistonBaseBlock

static bool piston_base_is_piston(state_id st) {
    return state_is(st, BLOCK_PISTON) || state_is(st, BLOCK_STICKY_PISTON);
}

// TRIGGER_EXTEND, TRIGGER_CONTRACT, TRIGGER_DROP
enum piston_trigger {
    PISTON_TRIGGER_EXTEND   = 0,
    PISTON_TRIGGER_CONTRACT = 1,
    PISTON_TRIGGER_DROP     = 2,
};

static direction piston_base_facing(state_id st) {
    return (direction) state_get(st, PROP_FACING);
}

static bool piston_base_is_sticky(state_id st) {
    return block_field_get(state_block(st), FIELD_PISTON_BASE_BLOCK_IS_STICKY) != 0;
}

// quasi-connectivity: the block above the piston counts as a power source position too
static bool piston_base_get_neighbor_signal(level & lv, block_pos pos, direction push_direction) {
    for (int32_t d = 0; d < 6; d++) {
        if (d != push_direction && level_has_signal(lv, pos_relative(pos, (direction) d), (direction) d)) {
            return true;
        }
    }
    if (level_has_signal(lv, pos, DIRECTION_DOWN)) {
        return true;
    }
    block_pos above = pos_relative(pos, DIRECTION_UP);
    for (int32_t d = 0; d < 6; d++) {
        if (d != DIRECTION_DOWN && level_has_signal(lv, pos_relative(above, (direction) d), (direction) d)) {
            return true;
        }
    }
    return false;
}

// the world border is far outside any case
static bool piston_base_is_pushable(state_id  st,
                                    block_pos pos,
                                    direction dir,
                                    bool      allow_destroyable,
                                    direction connection_direction) {
    if (pos.y < LEVEL_Y_MIN || pos.y > LEVEL_Y_MAX) {
        return false;
    }
    if (state_has(st, STATE_FLAG_AIR)) {
        return true;
    }
    if (state_is(st, BLOCK_OBSIDIAN) || state_is(st, BLOCK_CRYING_OBSIDIAN) || state_is(st, BLOCK_RESPAWN_ANCHOR) ||
        state_is(st, BLOCK_REINFORCED_DEEPSLATE)) {
        return false;
    }
    if ((dir == DIRECTION_DOWN && pos.y == LEVEL_Y_MIN) || (dir == DIRECTION_UP && pos.y == LEVEL_Y_MAX)) {
        return false;
    }
    if (!piston_base_is_piston(st)) {
        if (state_has(st, STATE_FLAG_UNBREAKABLE) || state_has(st, STATE_FLAG_PUSH_BLOCK)) {
            return false;
        }
        if (state_has(st, STATE_FLAG_PUSH_DESTROY)) {
            return allow_destroyable;
        }
        if (state_has(st, STATE_FLAG_PUSH_PUSH_ONLY)) {
            return dir == connection_direction;
        }
    } else if (state_get(st, PROP_EXTENDED)) {
        return false;
    }
    return !state_has(st, STATE_FLAG_BLOCK_ENTITY);
}

// PistonStructureResolver: the blocks a piston pushes and the blocks it breaks

struct piston_structure_resolver {
    block_pos              piston_pos;
    direction              piston_direction;
    bool                   extending;
    direction              push_direction;
    block_pos              start_pos;
    std::vector<block_pos> to_push;
    std::vector<block_pos> to_destroy;
};

// MAX_PUSH_DEPTH
static constexpr size_t PISTON_STRUCTURE_RESOLVER_MAX_PUSH_DEPTH = 12;

static bool piston_structure_resolver_is_sticky(state_id st) {
    return state_is(st, BLOCK_SLIME_BLOCK) || state_is(st, BLOCK_HONEY_BLOCK);
}

// slime and honey do not stick to each other
static bool piston_structure_resolver_can_stick_to_each_other(state_id a, state_id b) {
    if (state_is(a, BLOCK_HONEY_BLOCK) && state_is(b, BLOCK_SLIME_BLOCK)) {
        return false;
    }
    if (state_is(a, BLOCK_SLIME_BLOCK) && state_is(b, BLOCK_HONEY_BLOCK)) {
        return false;
    }
    return piston_structure_resolver_is_sticky(a) || piston_structure_resolver_is_sticky(b);
}

static int32_t piston_structure_resolver_index_of(const std::vector<block_pos> & list, block_pos pos) {
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i] == pos) {
            return (int32_t) i;
        }
    }
    return -1;
}

static bool piston_structure_resolver_add_block_line(level &                     lv,
                                                     piston_structure_resolver & r,
                                                     block_pos                   start,
                                                     direction                   dir);

static bool piston_structure_resolver_add_branching_blocks(level & lv, piston_structure_resolver & r, block_pos from) {
    state_id from_st = level_get_state(lv, from);
    for (int32_t d = 0; d < 6; d++) {
        direction dir = (direction) d;
        if (direction_get_axis(dir) == direction_get_axis(r.push_direction)) {
            continue;
        }
        block_pos neighbor_pos = pos_relative(from, dir);
        state_id  neighbor     = level_get_state(lv, neighbor_pos);
        if (piston_structure_resolver_can_stick_to_each_other(neighbor, from_st) &&
            !piston_structure_resolver_add_block_line(lv, r, neighbor_pos, dir)) {
            return false;
        }
    }
    return true;
}

// moves the line just added in front of the blocks it collided with
static void piston_structure_resolver_reorder_list_at_collision(piston_structure_resolver & r,
                                                                size_t                      blocks_added,
                                                                size_t                      collision) {
    std::vector<block_pos> & to_push = r.to_push;
    std::rotate(to_push.begin() + collision, to_push.end() - blocks_added, to_push.end());
}

static bool piston_structure_resolver_add_block_line(level &                     lv,
                                                     piston_structure_resolver & r,
                                                     block_pos                   start,
                                                     direction                   dir) {
    state_id next = level_get_state(lv, start);
    if (state_has(next, STATE_FLAG_AIR)) {
        return true;
    }
    if (!piston_base_is_pushable(next, start, r.push_direction, false, dir)) {
        return true;
    }
    if (start == r.piston_pos || piston_structure_resolver_index_of(r.to_push, start) >= 0) {
        return true;
    }
    direction back        = direction_opposite(r.push_direction);
    int32_t   block_count = 1;
    if (block_count + r.to_push.size() > PISTON_STRUCTURE_RESOLVER_MAX_PUSH_DEPTH) {
        return false;
    }
    // the sticky blocks behind start come along
    while (piston_structure_resolver_is_sticky(next)) {
        block_pos pos      = pos_relative_steps(start, back, block_count);
        state_id  previous = next;
        next               = level_get_state(lv, pos);
        if (state_has(next, STATE_FLAG_AIR) || !piston_structure_resolver_can_stick_to_each_other(previous, next) ||
            !piston_base_is_pushable(next, pos, r.push_direction, false, back) || pos == r.piston_pos) {
            break;
        }
        if (++block_count + r.to_push.size() > PISTON_STRUCTURE_RESOLVER_MAX_PUSH_DEPTH) {
            return false;
        }
    }
    size_t blocks_added = 0;
    for (int32_t i = block_count - 1; i >= 0; i--) {
        r.to_push.push_back(pos_relative_steps(start, back, i));
        blocks_added++;
    }
    for (int32_t i = 1;; i++) {
        block_pos pos       = pos_relative_steps(start, r.push_direction, i);
        int32_t   collision = piston_structure_resolver_index_of(r.to_push, pos);
        if (collision >= 0) {
            piston_structure_resolver_reorder_list_at_collision(r, blocks_added, collision);
            for (size_t j = 0; j <= collision + blocks_added; j++) {
                block_pos p = r.to_push[j];
                if (piston_structure_resolver_is_sticky(level_get_state(lv, p)) &&
                    !piston_structure_resolver_add_branching_blocks(lv, r, p)) {
                    return false;
                }
            }
            return true;
        }
        next = level_get_state(lv, pos);
        if (state_has(next, STATE_FLAG_AIR)) {
            return true;
        }
        if (!piston_base_is_pushable(next, pos, r.push_direction, true, r.push_direction) || pos == r.piston_pos) {
            return false;
        }
        if (state_has(next, STATE_FLAG_PUSH_DESTROY)) {
            r.to_destroy.push_back(pos);
            return true;
        }
        if (r.to_push.size() >= PISTON_STRUCTURE_RESOLVER_MAX_PUSH_DEPTH) {
            return false;
        }
        r.to_push.push_back(pos);
        blocks_added++;
    }
}

static piston_structure_resolver piston_structure_resolver_init(block_pos piston_pos, direction dir, bool extending) {
    piston_structure_resolver r;
    r.piston_pos       = piston_pos;
    r.piston_direction = dir;
    r.extending        = extending;
    r.push_direction   = extending ? dir : direction_opposite(dir);
    r.start_pos        = extending ? pos_relative(piston_pos, dir) : pos_relative_steps(piston_pos, dir, 2);
    return r;
}

static bool piston_structure_resolver_resolve(level & lv, piston_structure_resolver & r) {
    r.to_push.clear();
    r.to_destroy.clear();
    state_id next = level_get_state(lv, r.start_pos);
    if (!piston_base_is_pushable(next, r.start_pos, r.push_direction, false, r.piston_direction)) {
        if (r.extending && state_has(next, STATE_FLAG_PUSH_DESTROY)) {
            r.to_destroy.push_back(r.start_pos);
            return true;
        }
        return false;
    }
    if (!piston_structure_resolver_add_block_line(lv, r, r.start_pos, r.push_direction)) {
        return false;
    }
    // the list grows while it is walked
    for (size_t i = 0; i < r.to_push.size(); i++) {
        block_pos pos = r.to_push[i];
        if (piston_structure_resolver_is_sticky(level_get_state(lv, pos)) &&
            !piston_structure_resolver_add_branching_blocks(lv, r, pos)) {
            return false;
        }
    }
    return true;
}

// PistonMovingBlockEntity

static float piston_moving_block_entity_get_extended_progress(const block_entity * be, float progress) {
    return be->extending ? progress - 1.0f : 1.0f - progress;
}

// the moved block at its current offset; a retracting piston also has its base, and a moving head the short arm
static voxel_shape piston_moving_block_entity_get_collision_shape(level & lv, const block_entity * be) {
    voxel_shape head_shape;
    if (!be->extending && be->is_source_piston && piston_base_is_piston(be->moved_state)) {
        head_shape = voxel_shape_collision(state_set(be->moved_state, PROP_EXTENDED, 1));
    }
    // NOCLIP is set only while entities move
    state_id st = be->moved_state;
    if (be->is_source_piston) {
        st = state_set(block_default(BLOCK_PISTON_HEAD), PROP_FACING, be->dir);
        st = state_set(st, PROP_SHORT, be->extending != (1.0f - be->progress < 0.25f));
    }
    if (state_collision_shapes[st] == SHAPE_NONE) {
        level_unsupported(lv, "dynamic shape of %s", state_to_string(st).c_str());
        return voxel_shape();
    }
    float             extended_progress = piston_moving_block_entity_get_extended_progress(be, be->progress);
    const block_pos & step              = direction_defs[be->dir].step;
    voxel_shape       moved             = voxel_shape_move(voxel_shape_collision(st), step.x * extended_progress,
                                                           step.y * extended_progress, step.z * extended_progress);
    return shapes_join(head_shape, moved, BOOLEAN_OP_OR);
}

// Ends the move at once: the moved block lands in its new place.
void piston_moving_block_entity_final_tick(level & lv, block_entity * be) {
    if (be->progress_o >= 1.0f) {
        return;
    }
    be->progress   = 1.0f;
    be->progress_o = be->progress;
    level_remove_block_entity(lv, be->pos);
    if (state_is(level_get_state(lv, be->pos), BLOCK_MOVING_PISTON)) {
        state_id st =
            be->is_source_piston ? STATE_AIR : block_update_from_neighbour_shapes(lv, be->moved_state, be->pos);
        level_set_block(lv, be->pos, st, UPDATE_FLAG_ALL);
        level_neighbor_changed(lv, be->pos, state_block(st));
    }
}

// Entities are out of scope, so a tick only advances the progress (0, 0.5, 1) and then lands the block.
void piston_moving_block_entity_tick(level & lv, block_entity * be) {
    be->last_ticked = lv.game_time;
    be->progress_o  = be->progress;
    if (be->progress_o < 1.0f) {
        be->progress = std::min(be->progress + 0.5f, 1.0f);
        return;
    }
    block_pos pos = be->pos;
    level_remove_block_entity(lv, pos);
    if (!state_is(level_get_state(lv, pos), BLOCK_MOVING_PISTON)) {
        return;
    }
    state_id st = block_update_from_neighbour_shapes(lv, be->moved_state, pos);
    if (state_has(st, STATE_FLAG_AIR)) {
        // the block lands silently, then breaks as if a shape update removed it
        level_set_block(lv, pos, be->moved_state,
                        UPDATE_FLAG_INVISIBLE | UPDATE_FLAG_KNOWN_SHAPE | UPDATE_FLAG_MOVE_BY_PISTON |
                            UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
        block_update_or_destroy(lv, be->moved_state, st, pos, UPDATE_FLAG_ALL, UPDATE_LIMIT);
        return;
    }
    if (state_has_prop(st, PROP_WATERLOGGED) && state_get(st, PROP_WATERLOGGED)) {
        st = state_set(st, PROP_WATERLOGGED, 0);
    }
    level_set_block(lv, pos, st, UPDATE_FLAG_ALL | UPDATE_FLAG_MOVE_BY_PISTON);
    level_neighbor_changed(lv, pos, state_block(st));
}

// MovingPistonBlock

// its PistonMovingBlockEntity, or null
static block_entity * moving_piston_get_block_entity(level & lv, state_id st, block_pos pos) {
    return state_is(st, BLOCK_MOVING_PISTON) ? level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_PISTON) :
                                               nullptr;
}

voxel_shape moving_piston_get_collision_shape(level & lv, block_pos pos) {
    block_entity * be = moving_piston_get_block_entity(lv, level_get_state(lv, pos), pos);
    return be != nullptr ? piston_moving_block_entity_get_collision_shape(lv, be) : voxel_shape();
}

// the moving piston that stands in for a moved block, or for the base or arm of a retracting or extending piston
static state_id moving_piston_state(direction dir, bool sticky) {
    state_id st = state_set(block_default(BLOCK_MOVING_PISTON), PROP_FACING, dir);
    return state_set(st, PROP_PISTON_TYPE, sticky ? PISTON_TYPE_STICKY : PISTON_TYPE_NORMAL);
}

// PistonBaseBlock again: extending, retracting and moving the blocks

static void piston_base_check_if_extend(level & lv, state_id st, block_pos pos) {
    direction dir    = piston_base_facing(st);
    bool      extend = piston_base_get_neighbor_signal(lv, pos, dir);
    int32_t   block  = state_block(st);
    if (extend && !state_get(st, PROP_EXTENDED)) {
        piston_structure_resolver r = piston_structure_resolver_init(pos, dir, true);
        if (piston_structure_resolver_resolve(lv, r)) {
            level_block_event(lv, pos, block, PISTON_TRIGGER_EXTEND, dir);
        }
    } else if (!extend && state_get(st, PROP_EXTENDED)) {
        // a retraction while the pushed block is still early in its move drops it (no pulling back)
        block_pos      pushed_pos = pos_relative_steps(pos, dir, 2);
        state_id       pushed     = level_get_state(lv, pushed_pos);
        int32_t        event      = PISTON_TRIGGER_CONTRACT;
        block_entity * be         = moving_piston_get_block_entity(lv, pushed, pushed_pos);
        if (be != nullptr && state_get(pushed, PROP_FACING) == dir && be->extending &&
            (be->progress_o < 0.5f || lv.game_time == be->last_ticked || lv.handling_tick)) {
            event = PISTON_TRIGGER_DROP;
        }
        level_block_event(lv, pos, block, event, dir);
    }
}

static void piston_base_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    piston_base_check_if_extend(lv, st, pos);
}

static void piston_base_on_place(level & lv, state_id st, block_pos pos, state_id old, bool) {
    if (!state_is(old, state_block(st)) && level_get_block_entity(lv, pos) == nullptr) {
        piston_base_check_if_extend(lv, st, pos);
    }
}

static block_entity * piston_base_new_moving_block_entity(level &   lv,
                                                          block_pos pos,
                                                          state_id  st,
                                                          state_id  moved,
                                                          direction dir,
                                                          bool      extending,
                                                          bool      is_source_piston) {
    block_entity * be    = level_new_block_entity(lv, BLOCK_ENTITY_TYPE_PISTON, pos, st);
    be->moved_state      = moved;
    be->dir              = dir;
    be->extending        = extending;
    be->is_source_piston = is_source_piston;
    return be;
}

// PushReaction.NORMAL
static bool piston_base_push_reaction_is_normal(state_id st) {
    return !state_has(st, STATE_FLAG_PUSH_DESTROY) && !state_has(st, STATE_FLAG_PUSH_BLOCK) &&
           !state_has(st, STATE_FLAG_PUSH_IGNORE) && !state_has(st, STATE_FLAG_PUSH_PUSH_ONLY);
}

// removed blocks first, then moving pistons from the far end; the HashMap order decides which spots turn to air first
static bool piston_base_move_blocks(level & lv, state_id piston, block_pos piston_pos, direction dir, bool extending) {
    block_pos arm_pos = pos_relative(piston_pos, dir);
    if (!extending && state_is(level_get_state(lv, arm_pos), BLOCK_PISTON_HEAD)) {
        level_set_block(lv, arm_pos, STATE_AIR,
                        UPDATE_FLAG_INVISIBLE | UPDATE_FLAG_KNOWN_SHAPE | UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
    }
    piston_structure_resolver r = piston_structure_resolver_init(piston_pos, dir, extending);
    if (!piston_structure_resolver_resolve(lv, r)) {
        return false;
    }
    java_hash_map         delete_after_move;
    std::vector<state_id> to_push_shapes;
    for (block_pos pos : r.to_push) {
        state_id st = level_get_state(lv, pos);
        to_push_shapes.push_back(st);
        java_hash_map_put(delete_after_move, pos, st);
    }
    std::vector<state_id> to_update;  // removed states, by update order
    direction             push_direction = extending ? dir : direction_opposite(dir);
    for (size_t i = r.to_destroy.size(); i-- > 0;) {
        block_pos pos = r.to_destroy[i];
        state_id  st  = level_get_state(lv, pos);
        level_drop_resources(lv, st, pos,
                             state_has(st, STATE_FLAG_BLOCK_ENTITY) ? level_get_block_entity(lv, pos) : nullptr);
        level_set_block(lv, pos, STATE_AIR, UPDATE_FLAG_CLIENTS | UPDATE_FLAG_KNOWN_SHAPE);
        to_update.push_back(st);
    }
    for (size_t i = r.to_push.size(); i-- > 0;) {
        block_pos pos = r.to_push[i];
        state_id  st  = level_get_state(lv, pos);
        pos           = pos_relative(pos, push_direction);
        java_hash_map_remove(delete_after_move, pos);
        state_id moving = moving_piston_state(dir, false);
        level_set_block(lv, pos, moving,
                        UPDATE_FLAG_INVISIBLE | UPDATE_FLAG_MOVE_BY_PISTON | UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
        level_set_block_entity(
            lv, piston_base_new_moving_block_entity(lv, pos, moving, to_push_shapes[i], dir, extending, false));
        to_update.push_back(st);
    }
    if (extending) {
        bool     sticky = piston_base_is_sticky(piston);
        state_id head   = state_set(block_default(BLOCK_PISTON_HEAD), PROP_FACING, dir);
        head            = state_set(head, PROP_PISTON_TYPE, sticky ? PISTON_TYPE_STICKY : PISTON_TYPE_NORMAL);
        state_id moving = moving_piston_state(dir, sticky);
        java_hash_map_remove(delete_after_move, arm_pos);
        level_set_block(lv, arm_pos, moving,
                        UPDATE_FLAG_INVISIBLE | UPDATE_FLAG_MOVE_BY_PISTON | UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
        level_set_block_entity(lv, piston_base_new_moving_block_entity(lv, arm_pos, moving, head, dir, true, true));
    }
    std::vector<java_hash_map_entry> deleted = java_hash_map_entries(delete_after_move);
    for (const java_hash_map_entry & e : deleted) {
        level_set_block(lv, e.key, STATE_AIR,
                        UPDATE_FLAG_CLIENTS | UPDATE_FLAG_KNOWN_SHAPE | UPDATE_FLAG_MOVE_BY_PISTON);
    }
    for (const java_hash_map_entry & e : deleted) {
        block_update_indirect_neighbour_shapes(lv, (state_id) e.value, e.key, UPDATE_FLAG_CLIENTS, UPDATE_LIMIT);
        block_update_neighbour_shapes(lv, STATE_AIR, e.key, UPDATE_FLAG_CLIENTS, UPDATE_LIMIT);
        block_update_indirect_neighbour_shapes(lv, STATE_AIR, e.key, UPDATE_FLAG_CLIENTS, UPDATE_LIMIT);
    }
    size_t update_index = 0;
    for (size_t i = r.to_destroy.size(); i-- > 0;) {
        state_id  st  = to_update[update_index++];
        block_pos pos = r.to_destroy[i];
        block_affect_neighbors_after_removal(lv, st, pos, false);
        block_update_indirect_neighbour_shapes(lv, st, pos, UPDATE_FLAG_CLIENTS, UPDATE_LIMIT);
        level_update_neighbors_at(lv, pos, state_block(st));
    }
    for (size_t i = r.to_push.size(); i-- > 0;) {
        level_update_neighbors_at(lv, r.to_push[i], state_block(to_update[update_index++]));
    }
    if (extending) {
        level_update_neighbors_at(lv, arm_pos, BLOCK_PISTON_HEAD);
    }
    return true;
}

// the sound pitch draws from the level random
static bool piston_base_trigger_event(level & lv, state_id st, block_pos pos, int32_t b0, int32_t b1) {
    direction dir      = piston_base_facing(st);
    state_id  extended = state_set(st, PROP_EXTENDED, 1);
    bool      extend   = piston_base_get_neighbor_signal(lv, pos, dir);
    if (extend && (b0 == PISTON_TRIGGER_CONTRACT || b0 == PISTON_TRIGGER_DROP)) {
        level_set_block(lv, pos, extended, UPDATE_FLAG_CLIENTS);
        return false;
    }
    if (!extend && b0 == PISTON_TRIGGER_EXTEND) {
        return false;
    }
    if (b0 == PISTON_TRIGGER_EXTEND) {
        if (!piston_base_move_blocks(lv, st, pos, dir, true)) {
            return false;
        }
        level_set_block(lv, pos, extended, UPDATE_FLAG_CLIENTS | UPDATE_FLAG_MOVE_BY_PISTON | UPDATE_FLAG_NEIGHBORS);
        rng_next_float(lv.random);
        return true;
    }
    if (b0 != PISTON_TRIGGER_CONTRACT && b0 != PISTON_TRIGGER_DROP) {
        return true;
    }
    block_entity * previous = level_get_block_entity(lv, pos_relative(pos, dir));
    if (previous != nullptr && previous->type == BLOCK_ENTITY_TYPE_PISTON) {
        piston_moving_block_entity_final_tick(lv, previous);
    }
    bool     sticky = piston_base_is_sticky(st);
    state_id moving = moving_piston_state(dir, sticky);
    level_set_block(lv, pos, moving,
                    UPDATE_FLAG_INVISIBLE | UPDATE_FLAG_KNOWN_SHAPE | UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
    state_id base = state_set(block_default(state_block(st)), PROP_FACING, (direction) ((b1 & 7) % 6));
    level_set_block_entity(lv, piston_base_new_moving_block_entity(lv, pos, moving, base, dir, false, true));
    level_update_neighbors_at(lv, pos, BLOCK_MOVING_PISTON);
    block_update_neighbour_shapes(lv, moving, pos, UPDATE_FLAG_CLIENTS, UPDATE_LIMIT);
    if (sticky) {
        block_pos      two_pos      = pos_relative_steps(pos, dir, 2);
        state_id       moving_st    = level_get_state(lv, two_pos);
        bool           piston_piece = false;
        block_entity * be           = moving_piston_get_block_entity(lv, moving_st, two_pos);
        if (be != nullptr && be->dir == dir && be->extending) {
            piston_moving_block_entity_final_tick(lv, be);
            piston_piece = true;
        }
        if (!piston_piece) {
            if (b0 != PISTON_TRIGGER_CONTRACT || state_has(moving_st, STATE_FLAG_AIR) ||
                !piston_base_is_pushable(moving_st, two_pos, direction_opposite(dir), false, dir) ||
                (!piston_base_push_reaction_is_normal(moving_st) && !piston_base_is_piston(moving_st))) {
                level_remove_block(lv, pos_relative(pos, dir), false);
            } else {
                piston_base_move_blocks(lv, st, pos, dir, false);
            }
        }
    } else {
        level_remove_block(lv, pos_relative(pos, dir), false);
    }
    rng_next_float(lv.random);
    return true;
}

// PistonHeadBlock

static direction piston_head_facing(state_id st) {
    return (direction) state_get(st, PROP_FACING);
}

static bool piston_head_is_fitting_base(state_id arm, state_id base) {
    int32_t block = state_get(arm, PROP_PISTON_TYPE) == PISTON_TYPE_NORMAL ? BLOCK_PISTON : BLOCK_STICKY_PISTON;
    return state_is(base, block) && state_get(base, PROP_EXTENDED) &&
           state_get(base, PROP_FACING) == state_get(arm, PROP_FACING);
}

static block_pos piston_head_base_pos(state_id st, block_pos pos) {
    return pos_relative(pos, direction_opposite(piston_head_facing(st)));
}

// breaking the head breaks the base, with drops
static void piston_head_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool) {
    block_pos base_pos = piston_head_base_pos(st, pos);
    if (piston_head_is_fitting_base(st, level_get_state(lv, base_pos))) {
        level_destroy_block(lv, base_pos, true);
    }
}

// a base that is still moving in counts too
static bool piston_head_can_survive(level & lv, state_id st, block_pos pos) {
    state_id base = level_get_state(lv, piston_head_base_pos(st, pos));
    return piston_head_is_fitting_base(st, base) ||
           (state_is(base, BLOCK_MOVING_PISTON) && state_get(base, PROP_FACING) == state_get(st, PROP_FACING));
}

// the head passes updates on to its base
static void piston_head_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block, bool) {
    if (piston_head_can_survive(lv, st, pos)) {
        level_neighbor_changed(lv, piston_head_base_pos(st, pos), block);
    }
}

static state_id piston_head_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos, state_id) {
    return direction_opposite(dir) == piston_head_facing(st) && !piston_head_can_survive(lv, st, pos) ? STATE_AIR : st;
}

// clang-format off
const std::vector<block_hooks> piston_hooks = {
    { .vclass = VCLASS_PISTON_BASE_BLOCK,
      .neighbor_changed = piston_base_neighbor_changed,
      .on_place = piston_base_on_place,
      .trigger_event = piston_base_trigger_event },
    { .vclass = VCLASS_PISTON_HEAD_BLOCK,
      .update_shape = piston_head_update_shape,
      .neighbor_changed = piston_head_neighbor_changed,
      .affect_neighbors_after_removal = piston_head_affect_neighbors_after_removal,
      .can_survive = piston_head_can_survive },
};
// clang-format on
