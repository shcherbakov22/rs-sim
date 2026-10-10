// TripWireBlock: no entities here, so a wire is never pressed; changes reach the hooks south and west of it

#include "blocks/handlers.h"
#include "engine/level.h"

static bool trip_wire_should_connect_to(state_id st, state_id neighbor, direction dir) {
    if (state_is(neighbor, block_field_get(state_block(st), FIELD_TRIP_WIRE_BLOCK_HOOK))) {
        return (direction) state_get(neighbor, PROP_HORIZONTAL_FACING) == direction_opposite(dir);
    }
    return state_is(neighbor, state_block(st));
}

state_id trip_wire_update_shape(state_id st, direction dir, state_id neighbor) {
    if (!direction_is_horizontal(dir)) {
        return st;
    }
    return state_set(st, cross_collision_property_by_direction(dir), trip_wire_should_connect_to(st, neighbor, dir));
}

// a hook facing back along the line recalculates with this wire's new state
static void trip_wire_update_source(level & lv, block_pos pos, state_id st) {
    for (direction dir : { DIRECTION_SOUTH, DIRECTION_WEST }) {
        for (int32_t i = 1; i < 42; i++) {
            block_pos test_pos = pos_relative_steps(pos, dir, i);
            state_id  test     = level_get_state(lv, test_pos);
            if (state_is(test, block_field_get(state_block(st), FIELD_TRIP_WIRE_BLOCK_HOOK))) {
                if ((direction) state_get(test, PROP_HORIZONTAL_FACING) == direction_opposite(dir)) {
                    trip_wire_hook_calculate_state(lv, test_pos, test, false, true, i, st);
                }
                break;
            }
            if (!state_is(test, state_block(st))) {
                break;
            }
        }
    }
}

void trip_wire_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    if (state_block(old) != state_block(st)) {
        trip_wire_update_source(lv, pos, st);
    }
}

// a removed wire reports itself as pressed
void trip_wire_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (!moved_by_piston) {
        trip_wire_update_source(lv, pos, state_set(st, PROP_POWERED, 1));
    }
}

// checkPressed with no entity on the wire: a pressed wire is released
void trip_wire_tick(level & lv, block_pos pos) {
    state_id st = level_get_state(lv, pos);
    if (state_get(st, PROP_POWERED)) {
        st = state_set(st, PROP_POWERED, 0);
        level_set_block(lv, pos, st, UPDATE_FLAG_ALL);
        trip_wire_update_source(lv, pos, st);
    }
}
