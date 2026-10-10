// TripWireHookBlock: a hook looks along its facing for a wire line ending in a hook that faces back

#include "blocks/handlers.h"
#include "engine/level.h"

#include <array>

bool trip_wire_hook_can_survive(level & lv, state_id st, block_pos pos) {
    direction facing = (direction) state_get(st, PROP_HORIZONTAL_FACING);
    block_pos behind = pos_relative(pos, direction_opposite(facing));
    return block_is_face_sturdy(lv, level_get_state(lv, behind), behind, facing, SUPPORT_TYPE_FULL);
}

state_id trip_wire_hook_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    if (direction_opposite(dir) == (direction) state_get(st, PROP_HORIZONTAL_FACING) &&
        !block_can_survive(lv, st, pos)) {
        return STATE_AIR;
    }
    return st;
}

static void trip_wire_hook_notify_neighbors(level & lv, int32_t block, block_pos pos, direction dir) {
    level_update_neighbors_at(lv, pos, block);
    level_update_neighbors_at(lv, pos_relative(pos, direction_opposite(dir)), block);
}

// of the four sounds only the detach sound (power unchanged) draws from the level random, for its pitch
static void trip_wire_hook_emit_state(level & lv, bool attached, bool powered, bool was_attached, bool was_powered) {
    if (powered == was_powered && !attached && was_attached) {
        rng_next_float(lv.random);
    }
}

// wire_source is the distance of a wire that changed (its new state is wire_source_state), -1 for none
void trip_wire_hook_calculate_state(level &   lv,
                                    block_pos pos,
                                    state_id  st,
                                    bool      being_destroyed,
                                    bool      can_update,
                                    int32_t   wire_source,
                                    state_id  wire_source_state) {
    direction               facing       = (direction) state_get(st, PROP_HORIZONTAL_FACING);
    bool                    was_attached = state_get(st, PROP_ATTACHED);
    bool                    was_powered  = state_get(st, PROP_POWERED);
    int32_t                 block        = state_block(st);
    bool                    attached     = !being_destroyed;
    bool                    powered      = false;
    int32_t                 receiver_pos = 0;
    std::array<int32_t, 42> wire_states;  // -1 for null
    wire_states.fill(-1);
    for (int32_t i = 1; i < 42; i++) {
        block_pos test_pos   = pos_relative_steps(pos, facing, i);
        state_id  wire_state = level_get_state(lv, test_pos);
        if (state_is(wire_state, BLOCK_TRIPWIRE_HOOK)) {
            if ((direction) state_get(wire_state, PROP_HORIZONTAL_FACING) == direction_opposite(facing)) {
                receiver_pos = i;
            }
            break;
        }
        if (!state_is(wire_state, BLOCK_TRIPWIRE) && i != wire_source) {
            attached = false;
            continue;
        }
        if (i == wire_source) {
            wire_state = wire_source_state;
        }
        bool wire_armed = !state_get(wire_state, PROP_DISARMED);
        powered |= wire_armed && state_get(wire_state, PROP_POWERED);
        wire_states[i] = wire_state;
        if (i == wire_source) {
            level_schedule_tick(lv, pos, block, 10);
            attached &= wire_armed;
        }
    }
    attached &= receiver_pos > 1;
    powered &= attached;
    state_id next =
        state_set(state_set(block_defs[block].state_default, PROP_ATTACHED, attached), PROP_POWERED, powered);
    if (receiver_pos > 0) {
        block_pos test_pos = pos_relative_steps(pos, facing, receiver_pos);
        direction opposite = direction_opposite(facing);
        level_set_block(lv, test_pos, state_set(next, PROP_HORIZONTAL_FACING, opposite), UPDATE_FLAG_ALL);
        trip_wire_hook_notify_neighbors(lv, block, test_pos, opposite);
        trip_wire_hook_emit_state(lv, attached, powered, was_attached, was_powered);
    }
    trip_wire_hook_emit_state(lv, attached, powered, was_attached, was_powered);
    if (!being_destroyed) {
        level_set_block(lv, pos, state_set(next, PROP_HORIZONTAL_FACING, facing), UPDATE_FLAG_ALL);
        if (can_update) {
            trip_wire_hook_notify_neighbors(lv, block, pos, facing);
        }
    }
    if (was_attached == attached) {
        return;
    }
    for (int32_t i = 1; i < receiver_pos; i++) {
        block_pos test_pos = pos_relative_steps(pos, facing, i);
        if (wire_states[i] < 0) {
            continue;
        }
        state_id test_state = level_get_state(lv, test_pos);
        if (state_is(test_state, BLOCK_TRIPWIRE) || state_is(test_state, BLOCK_TRIPWIRE_HOOK)) {
            level_set_block(lv, test_pos, state_set((state_id) wire_states[i], PROP_ATTACHED, attached),
                            UPDATE_FLAG_ALL);
        }
    }
}

void trip_wire_hook_tick(level & lv, state_id st, block_pos pos) {
    trip_wire_hook_calculate_state(lv, pos, st, false, true, -1, -1);
}

void trip_wire_hook_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (moved_by_piston) {
        return;
    }
    bool attached = state_get(st, PROP_ATTACHED);
    bool powered  = state_get(st, PROP_POWERED);
    if (attached || powered) {
        trip_wire_hook_calculate_state(lv, pos, st, true, false, -1, -1);
    }
    if (powered) {
        trip_wire_hook_notify_neighbors(lv, state_block(st), pos, (direction) state_get(st, PROP_HORIZONTAL_FACING));
    }
}

int32_t trip_wire_hook_get_signal(state_id st) {
    return state_get(st, PROP_POWERED) ? 15 : 0;
}

int32_t trip_wire_hook_get_direct_signal(state_id st, direction dir) {
    return state_get(st, PROP_POWERED) && (direction) state_get(st, PROP_HORIZONTAL_FACING) == dir ? 15 : 0;
}
