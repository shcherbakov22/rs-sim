// FenceGateBlock: in_wall follows walls on the sides; a signal opens it

#include "blocks/handlers.h"
#include "engine/level.h"

// only the neighbors along the gate's width decide in_wall
state_id fence_gate_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor) {
    direction facing = (direction) state_get(st, PROP_HORIZONTAL_FACING);
    if (direction_get_axis(direction_clockwise(facing)) != direction_get_axis(dir)) {
        return st;
    }
    bool in_wall = state_has(neighbor, STATE_FLAG_TAG_WALLS) ||
                   state_has(level_get_state(lv, pos_relative(pos, direction_opposite(dir))), STATE_FLAG_TAG_WALLS);
    return state_set(st, PROP_IN_WALL, in_wall);
}

void fence_gate_neighbor_changed(level & lv, state_id st, block_pos pos) {
    bool has_power = level_has_neighbor_signal(lv, pos);
    if (state_get(st, PROP_POWERED) == has_power) {
        return;
    }
    level_set_block(lv, pos, state_set(state_set(st, PROP_POWERED, has_power), PROP_OPEN, has_power),
                    UPDATE_FLAG_CLIENTS);
    if (state_get(st, PROP_OPEN) != has_power) {
        rng_next_float(lv.random);
    }
}
