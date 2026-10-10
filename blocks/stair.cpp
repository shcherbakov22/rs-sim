// StairBlock: the shape turns into a corner when stairs of the same half sit behind or in front across the axis

#include "blocks/handlers.h"
#include "engine/level.h"

static bool stair_is_stairs(state_id st) {
    return block_instanceof(state_block(st), VCLASS_STAIR_BLOCK);
}

static direction stair_facing(state_id st) {
    return (direction) state_get(st, PROP_HORIZONTAL_FACING);
}

// no stairs of the same facing and half on that side
static bool stair_can_take_shape(level & lv, state_id st, block_pos pos, direction neighbor) {
    state_id neighbor_state = level_get_state(lv, pos_relative(pos, neighbor));
    return !stair_is_stairs(neighbor_state) || stair_facing(neighbor_state) != stair_facing(st) ||
           state_get(neighbor_state, PROP_HALF) != state_get(st, PROP_HALF);
}

static int32_t stair_get_stairs_shape(level & lv, state_id st, block_pos pos) {
    direction facing = stair_facing(st);
    state_id  behind = level_get_state(lv, pos_relative(pos, facing));
    if (stair_is_stairs(behind) && state_get(st, PROP_HALF) == state_get(behind, PROP_HALF)) {
        direction behind_facing = stair_facing(behind);
        if (direction_get_axis(behind_facing) != direction_get_axis(facing) &&
            stair_can_take_shape(lv, st, pos, direction_opposite(behind_facing))) {
            return behind_facing == direction_counter_clockwise(facing) ? STAIRS_SHAPE_OUTER_LEFT :
                                                                          STAIRS_SHAPE_OUTER_RIGHT;
        }
    }
    state_id front = level_get_state(lv, pos_relative(pos, direction_opposite(facing)));
    if (stair_is_stairs(front) && state_get(st, PROP_HALF) == state_get(front, PROP_HALF)) {
        direction front_facing = stair_facing(front);
        if (direction_get_axis(front_facing) != direction_get_axis(facing) &&
            stair_can_take_shape(lv, st, pos, front_facing)) {
            return front_facing == direction_counter_clockwise(facing) ? STAIRS_SHAPE_INNER_LEFT :
                                                                         STAIRS_SHAPE_INNER_RIGHT;
        }
    }
    return STAIRS_SHAPE_STRAIGHT;
}

state_id stair_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    if (state_get(st, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
    }
    if (!direction_is_horizontal(dir)) {
        return st;
    }
    return state_set(st, PROP_STAIRS_SHAPE, stair_get_stairs_shape(lv, st, pos));
}
