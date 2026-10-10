// PistonBaseBlock

#include "blocks/handlers.h"
#include "engine/java-hash-map.h"
#include "engine/level.h"

// PistonBaseBlock.TRIGGER_EXTEND, TRIGGER_CONTRACT, TRIGGER_DROP
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

static bool piston_base_is_piston(state_id st) {
    return state_is(st, BLOCK_PISTON) || state_is(st, BLOCK_STICKY_PISTON);
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
        block_pos      pushed_pos = pos_relative(pos_relative(pos, dir), dir);
        state_id       pushed     = level_get_state(lv, pushed_pos);
        int32_t        event      = PISTON_TRIGGER_CONTRACT;
        block_entity * be         = nullptr;
        if (state_is(pushed, BLOCK_MOVING_PISTON) && state_get(pushed, PROP_FACING) == dir) {
            be = level_get_block_entity(lv, pushed_pos);
        }
        if (be != nullptr && be->type == BLOCK_ENTITY_TYPE_PISTON && be->extending &&
            (be->progress_o < 0.5f || lv.game_time == be->last_ticked || lv.handling_tick)) {
            event = PISTON_TRIGGER_DROP;
        }
        level_block_event(lv, pos, block, event, dir);
    }
}

void piston_base_neighbor_changed(level & lv, state_id st, block_pos pos) {
    piston_base_check_if_extend(lv, st, pos);
}

void piston_base_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    if (!state_is(old, state_block(st)) && level_get_block_entity(lv, pos) == nullptr) {
        piston_base_check_if_extend(lv, st, pos);
    }
}

// the world border is far outside any case
bool piston_base_is_pushable(state_id  st,
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

static state_id piston_base_moving_piston_state(direction dir, bool sticky) {
    state_id st = state_set(block_default(BLOCK_MOVING_PISTON), PROP_FACING, dir);
    return state_set(st, PROP_PISTON_TYPE, sticky ? PISTON_TYPE_STICKY : PISTON_TYPE_NORMAL);
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
        state_id moving = state_set(block_default(BLOCK_MOVING_PISTON), PROP_FACING, dir);
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
        state_id moving = piston_base_moving_piston_state(dir, sticky);
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
bool piston_base_trigger_event(level & lv, state_id st, block_pos pos, int32_t b0, int32_t b1) {
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
    state_id moving = piston_base_moving_piston_state(dir, sticky);
    level_set_block(lv, pos, moving,
                    UPDATE_FLAG_INVISIBLE | UPDATE_FLAG_KNOWN_SHAPE | UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
    state_id base = state_set(block_default(state_block(st)), PROP_FACING, (direction) ((b1 & 7) % 6));
    level_set_block_entity(lv, piston_base_new_moving_block_entity(lv, pos, moving, base, dir, false, true));
    level_update_neighbors_at(lv, pos, BLOCK_MOVING_PISTON);
    block_update_neighbour_shapes(lv, moving, pos, UPDATE_FLAG_CLIENTS, UPDATE_LIMIT);
    if (sticky) {
        block_pos      two_pos      = pos_relative(pos_relative(pos, dir), dir);
        state_id       moving_st    = level_get_state(lv, two_pos);
        bool           piston_piece = false;
        block_entity * be           = nullptr;
        if (state_is(moving_st, BLOCK_MOVING_PISTON)) {
            be = level_get_block_entity(lv, two_pos);
        }
        if (be != nullptr && be->type == BLOCK_ENTITY_TYPE_PISTON && be->dir == dir && be->extending) {
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

// the piston faces the player
state_id piston_base_get_state_for_placement(int32_t block) {
    state_id st = state_set(block_default(block), PROP_FACING, direction_opposite(player_nearest_directions[0]));
    return state_set(st, PROP_EXTENDED, 0);
}

void piston_base_set_placed_by(level & lv, state_id st, block_pos pos) {
    piston_base_check_if_extend(lv, st, pos);
}
