// WallBlock: sides connect like bars; the block above decides tall sides and the post

#include "blocks/handlers.h"
#include "core/voxel-shape.h"
#include "engine/level.h"

// WallBlock.PROPERTY_BY_DIRECTION
static prop_id wall_property_by_direction(direction dir) {
    switch (dir) {
        case DIRECTION_NORTH:
            return PROP_NORTH_WALL;
        case DIRECTION_EAST:
            return PROP_EAST_WALL;
        case DIRECTION_SOUTH:
            return PROP_SOUTH_WALL;
        default:
            return PROP_WEST_WALL;
    }
}

// TEST_SHAPES_WALL: a 2 wide strip from the center to the side, full height
static voxel_shape wall_test_shape_wall(direction dir) {
    switch (dir) {
        case DIRECTION_NORTH:
            return shapes_box(7.0 / 16, 0.0, 0.0, 9.0 / 16, 1.0, 9.0 / 16);
        case DIRECTION_EAST:
            return shapes_box(7.0 / 16, 0.0, 7.0 / 16, 1.0, 1.0, 9.0 / 16);
        case DIRECTION_SOUTH:
            return shapes_box(7.0 / 16, 0.0, 7.0 / 16, 9.0 / 16, 1.0, 1.0);
        default:
            return shapes_box(0.0, 0.0, 7.0 / 16, 9.0 / 16, 1.0, 9.0 / 16);
    }
}

static bool wall_is_covered(const voxel_shape & above_shape, const voxel_shape & test_shape) {
    return !shapes_join_is_not_empty(test_shape, above_shape, BOOLEAN_OP_ONLY_FIRST);
}

static bool wall_connects_to(state_id st, bool face_solid, direction dir) {
    bool connected_fence_gate = block_instanceof(state_block(st), VCLASS_FENCE_GATE_BLOCK) &&
                                direction_get_axis((direction) state_get(st, PROP_HORIZONTAL_FACING)) ==
                                    direction_get_axis(direction_clockwise(dir));
    return state_has(st, STATE_FLAG_TAG_WALLS) || (!state_has(st, STATE_FLAG_CONNECTION_EXCEPTION) && face_solid) ||
           block_instanceof(state_block(st), VCLASS_IRON_BARS_BLOCK) || connected_fence_gate;
}

static bool wall_should_raise_post(state_id st, state_id top, const voxel_shape & above_shape) {
    if (block_instanceof(state_block(top), VCLASS_WALL_BLOCK) && state_get(top, PROP_UP)) {
        return true;
    }
    int32_t north      = state_get(st, PROP_NORTH_WALL);
    int32_t south      = state_get(st, PROP_SOUTH_WALL);
    int32_t east       = state_get(st, PROP_EAST_WALL);
    int32_t west       = state_get(st, PROP_WEST_WALL);
    bool    south_none = south == WALL_SIDE_NONE;
    bool    west_none  = west == WALL_SIDE_NONE;
    bool    east_none  = east == WALL_SIDE_NONE;
    bool    north_none = north == WALL_SIDE_NONE;
    bool    has_corner =
        (north_none && south_none && west_none && east_none) || north_none != south_none || west_none != east_none;
    if (has_corner) {
        return true;
    }
    bool has_high_wall =
        (north == WALL_SIDE_TALL && south == WALL_SIDE_TALL) || (east == WALL_SIDE_TALL && west == WALL_SIDE_TALL);
    if (has_high_wall) {
        return false;
    }
    return state_has(top, STATE_FLAG_TAG_WALL_POST_OVERRIDE) ||
           wall_is_covered(above_shape, shapes_box(7.0 / 16, 0.0, 7.0 / 16, 9.0 / 16, 1.0, 9.0 / 16));
}

// the private WallBlock.updateShape: connected[] is indexed by direction
static state_id wall_update_shape_from(level &    lv,
                                       state_id   st,
                                       block_pos  top_pos,
                                       state_id   top,
                                       const bool connected[6]) {
    voxel_shape above_shape = voxel_shape_get_face_shape(block_get_collision_shape(lv, top, top_pos), DIRECTION_DOWN);
    for (direction dir : DIRECTION_HORIZONTAL) {
        int32_t side = WALL_SIDE_NONE;
        if (connected[dir]) {
            side = wall_is_covered(above_shape, wall_test_shape_wall(dir)) ? WALL_SIDE_TALL : WALL_SIDE_LOW;
        }
        st = state_set(st, wall_property_by_direction(dir), side);
    }
    return state_set(st, PROP_UP, wall_should_raise_post(st, top, above_shape));
}

state_id wall_update_shape(level &   lv,
                           state_id  st,
                           block_pos pos,
                           direction dir,
                           block_pos neighbor_pos,
                           state_id  neighbor) {
    if (dir == DIRECTION_DOWN) {
        return st;
    }
    bool connected[6] = {};
    for (direction side : DIRECTION_HORIZONTAL) {
        connected[side] = state_get(st, wall_property_by_direction(side)) != WALL_SIDE_NONE;
    }
    if (dir == DIRECTION_UP) {
        return wall_update_shape_from(lv, st, neighbor_pos, neighbor, connected);
    }
    // sideUpdate
    direction opposite = direction_opposite(dir);
    connected[dir]     = wall_connects_to(
        neighbor, block_is_face_sturdy(lv, neighbor, neighbor_pos, opposite, SUPPORT_TYPE_FULL), opposite);
    block_pos above = pos_relative(pos, DIRECTION_UP);
    return wall_update_shape_from(lv, st, above, level_get_state(lv, above), connected);
}
