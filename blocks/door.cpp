// DoorBlock: two halves that copy each other through updateShape; a signal opens it

#include "blocks/handlers.h"
#include "engine/level.h"

// the sound pitch draws a float
static void door_play_sound(level & lv) {
    rng_next_float(lv.random);
}

bool door_can_survive(level & lv, state_id st, block_pos pos) {
    block_pos below       = pos_relative(pos, DIRECTION_DOWN);
    state_id  below_state = level_get_state(lv, below);
    if (state_get(st, PROP_DOUBLE_BLOCK_HALF) == DOUBLE_BLOCK_HALF_LOWER) {
        return block_is_face_sturdy(lv, below_state, below, DIRECTION_UP, SUPPORT_TYPE_FULL);
    }
    return state_block(below_state) == state_block(st);
}

// toward the other half the door takes the other half's state; a half without its partner breaks
state_id door_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor) {
    int32_t half = state_get(st, PROP_DOUBLE_BLOCK_HALF);
    if (direction_get_axis(dir) != DIRECTION_AXIS_Y || (half == DOUBLE_BLOCK_HALF_LOWER) != (dir == DIRECTION_UP)) {
        if (half == DOUBLE_BLOCK_HALF_LOWER && dir == DIRECTION_DOWN && !door_can_survive(lv, st, pos)) {
            return STATE_AIR;
        }
        return st;
    }
    if (block_instanceof(state_block(neighbor), VCLASS_DOOR_BLOCK) &&
        state_get(neighbor, PROP_DOUBLE_BLOCK_HALF) != half) {
        return state_set(neighbor, PROP_DOUBLE_BLOCK_HALF, half);
    }
    return STATE_AIR;
}

// either half reads the signal at both; an update from a block of the same door type is ignored
void door_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block) {
    direction other  = state_get(st, PROP_DOUBLE_BLOCK_HALF) == DOUBLE_BLOCK_HALF_LOWER ? DIRECTION_UP : DIRECTION_DOWN;
    bool      signal = level_has_neighbor_signal(lv, pos) || level_has_neighbor_signal(lv, pos_relative(pos, other));
    if (block == state_block(st) || signal == (bool) state_get(st, PROP_POWERED)) {
        return;
    }
    if (signal != (bool) state_get(st, PROP_OPEN)) {
        door_play_sound(lv);
    }
    level_set_block(lv, pos, state_set(state_set(st, PROP_POWERED, signal), PROP_OPEN, signal), UPDATE_FLAG_CLIENTS);
}
