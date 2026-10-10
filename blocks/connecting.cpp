// Connecting blocks: fences, iron bars and panes, walls and stairs fit their shape to their neighbors

#include "blocks/handlers.h"
#include "engine/level.h"

// CrossCollisionBlock.PROPERTY_BY_DIRECTION and WallBlock's, indexed by direction
static constexpr prop_id CROSS_COLLISION_PROPERTY_BY_DIRECTION[6] = { PROP_COUNT, PROP_COUNT, PROP_NORTH,
                                                                      PROP_SOUTH, PROP_WEST,  PROP_EAST };
static constexpr prop_id WALL_PROPERTY_BY_DIRECTION[6]            = { PROP_COUNT,      PROP_COUNT,     PROP_NORTH_WALL,
                                                                      PROP_SOUTH_WALL, PROP_WEST_WALL, PROP_EAST_WALL };

// FenceGateBlock.connectsToDirection
static bool fence_gate_connects_to_direction(state_id st, direction dir) {
    return block_instanceof(state_block(st), VCLASS_FENCE_GATE_BLOCK) &&
           direction_get_axis((direction) state_get(st, PROP_HORIZONTAL_FACING)) ==
               direction_get_axis(direction_clockwise(dir));
}

// the neighbor's face toward pos is sturdy (asked first, as vanilla does) and no Block.isExceptionForConnection
static bool connecting_face_solid(level & lv, block_pos neighbor_pos, state_id neighbor, direction dir) {
    bool sturdy = block_is_face_sturdy(lv, neighbor, neighbor_pos, direction_opposite(dir), SUPPORT_TYPE_FULL);
    return sturdy && !state_has(neighbor, STATE_FLAG_CONNECTION_EXCEPTION);
}

// FenceBlock: a side connects to a solid face, a fence of the same kind (wooden or not) or a gate across it
static state_id fence_update_shape(level &  lv,
                                   state_id st,
                                   block_pos,
                                   direction dir,
                                   block_pos neighbor_pos,
                                   state_id  neighbor) {
    if (!direction_is_horizontal(dir)) {
        return st;
    }
    bool same_fence = state_has(neighbor, STATE_FLAG_TAG_FENCES) &&
                      state_has(neighbor, STATE_FLAG_TAG_WOODEN_FENCES) == state_has(st, STATE_FLAG_TAG_WOODEN_FENCES);
    bool connects   = connecting_face_solid(lv, neighbor_pos, neighbor, dir) || same_fence ||
                      fence_gate_connects_to_direction(neighbor, direction_opposite(dir));
    return state_set(st, CROSS_COLLISION_PROPERTY_BY_DIRECTION[dir], connects);
}

// IronBarsBlock (bars and panes): a side connects to a solid face, other bars or a wall
static state_id iron_bars_update_shape(level &  lv,
                                       state_id st,
                                       block_pos,
                                       direction dir,
                                       block_pos neighbor_pos,
                                       state_id  neighbor) {
    if (!direction_is_horizontal(dir)) {
        return st;
    }
    bool attaches = connecting_face_solid(lv, neighbor_pos, neighbor, dir) ||
                    block_instanceof(state_block(neighbor), VCLASS_IRON_BARS_BLOCK) ||
                    state_has(neighbor, STATE_FLAG_TAG_WALLS);
    return state_set(st, CROSS_COLLISION_PROPERTY_BY_DIRECTION[dir], attaches);
}

// WallBlock: sides connect like bars; the block above decides tall sides and the post

// TEST_SHAPES_WALL: a 2 wide strip from the center to the side, full height
static shape_box wall_test_shape_wall(direction dir) {
    switch (dir) {
        case DIRECTION_NORTH:
            return block_box(7, 0, 0, 9, 16, 9);
        case DIRECTION_EAST:
            return block_box(7, 0, 7, 16, 16, 9);
        case DIRECTION_SOUTH:
            return block_box(7, 0, 7, 9, 16, 16);
        default:
            return block_box(0, 0, 7, 9, 16, 9);
    }
}

static bool wall_should_raise_post(state_id st, state_id top, const shape_face & above_shape) {
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
           shape_face_covers(above_shape, block_box(7, 0, 7, 9, 16, 9));
}

// the private WallBlock.updateShape: connected[] is indexed by direction
static state_id wall_update_shape_from(level &    lv,
                                       state_id   st,
                                       block_pos  top_pos,
                                       state_id   top,
                                       const bool connected[6]) {
    shape_face above_shape = shape_get_face(block_get_collision_shape(lv, top, top_pos), DIRECTION_DOWN);
    for (direction dir : DIRECTION_HORIZONTAL) {
        int32_t side = WALL_SIDE_NONE;
        if (connected[dir]) {
            side = shape_face_covers(above_shape, wall_test_shape_wall(dir)) ? WALL_SIDE_TALL : WALL_SIDE_LOW;
        }
        st = state_set(st, WALL_PROPERTY_BY_DIRECTION[dir], side);
    }
    return state_set(st, PROP_UP, wall_should_raise_post(st, top, above_shape));
}

static state_id wall_update_shape(level &   lv,
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
        connected[side] = state_get(st, WALL_PROPERTY_BY_DIRECTION[side]) != WALL_SIDE_NONE;
    }
    if (dir == DIRECTION_UP) {
        return wall_update_shape_from(lv, st, neighbor_pos, neighbor, connected);
    }
    // sideUpdate: WallBlock.connectsTo
    bool face_solid = connecting_face_solid(lv, neighbor_pos, neighbor, dir);
    connected[dir]  = state_has(neighbor, STATE_FLAG_TAG_WALLS) || face_solid ||
                      block_instanceof(state_block(neighbor), VCLASS_IRON_BARS_BLOCK) ||
                      fence_gate_connects_to_direction(neighbor, direction_opposite(dir));
    block_pos above = pos_relative(pos, DIRECTION_UP);
    return wall_update_shape_from(lv, st, above, level_get_state(lv, above), connected);
}

// StairBlock: the shape turns into a corner when stairs of the same half sit behind or in front across the axis

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

static state_id stair_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos, state_id) {
    if (!direction_is_horizontal(dir)) {
        return st;
    }
    return state_set(st, PROP_STAIRS_SHAPE, stair_get_stairs_shape(lv, st, pos));
}

// clang-format off
const std::vector<block_hooks> connecting_hooks = {
    { .vclass = VCLASS_FENCE_BLOCK, .update_shape = fence_update_shape },
    { .vclass = VCLASS_IRON_BARS_BLOCK, .update_shape = iron_bars_update_shape },
    { .vclass = VCLASS_WALL_BLOCK, .update_shape = wall_update_shape },
    { .vclass = VCLASS_STAIR_BLOCK, .update_shape = stair_update_shape },
};
// clang-format on
