#pragma once

#include "core/common.h"
#include "gen/blocks.h"

#include <string_view>

// the generated enum direction is vanilla's Direction in 3D data value order
static_assert(DIRECTION_DOWN == 0 && DIRECTION_EAST == 5);

// Direction.Plane.HORIZONTAL and VERTICAL
inline constexpr direction DIRECTION_HORIZONTAL[4] = { DIRECTION_NORTH, DIRECTION_EAST, DIRECTION_SOUTH,
                                                       DIRECTION_WEST };
inline constexpr direction DIRECTION_VERTICAL[2]   = { DIRECTION_UP, DIRECTION_DOWN };

struct direction_def {
    const char * name;
    block_pos    step;
};

inline constexpr direction_def direction_defs[] = {
    { "down",  { 0, -1, 0 } },
    { "up",    { 0, 1, 0 }  },
    { "north", { 0, 0, -1 } },
    { "south", { 0, 0, 1 }  },
    { "west",  { -1, 0, 0 } },
    { "east",  { 1, 0, 0 }  },
};

inline bool direction_parse(std::string_view name, direction & out) {
    for (int32_t i = 0; i < 6; i++) {
        if (name == direction_defs[i].name) {
            out = (direction) i;
            return true;
        }
    }
    return false;
}

inline direction direction_opposite(direction dir) {
    return (direction) (dir ^ 1);
}

inline block_pos pos_relative(block_pos pos, direction dir) {
    const block_pos & s = direction_defs[dir].step;
    return { pos.x + s.x, pos.y + s.y, pos.z + s.z };
}

// BlockPos.relative(Direction, int)
inline block_pos pos_relative_steps(block_pos pos, direction dir, int32_t steps) {
    const block_pos & s = direction_defs[dir].step;
    return { pos.x + s.x * steps, pos.y + s.y * steps, pos.z + s.z * steps };
}

inline bool direction_is_horizontal(direction dir) {
    return dir >= DIRECTION_NORTH;
}

inline direction_axis direction_get_axis(direction dir) {
    static constexpr direction_axis axes[6] = { DIRECTION_AXIS_Y, DIRECTION_AXIS_Y, DIRECTION_AXIS_Z,
                                                DIRECTION_AXIS_Z, DIRECTION_AXIS_X, DIRECTION_AXIS_X };
    return axes[dir];
}

// Direction.getClockWise, horizontal only
inline direction direction_clockwise(direction dir) {
    static constexpr direction next[6] = { DIRECTION_DOWN, DIRECTION_UP,    DIRECTION_EAST,
                                           DIRECTION_WEST, DIRECTION_NORTH, DIRECTION_SOUTH };
    TMC_ASSERT(direction_is_horizontal(dir));
    return next[dir];
}

inline direction direction_counter_clockwise(direction dir) {
    return direction_opposite(direction_clockwise(dir));
}

// Direction.get2DDataValue: south, west, north, east; -1 for up and down
inline int32_t direction_get_2d_data_value(direction dir) {
    static constexpr int32_t values[6] = { -1, -1, 2, 0, 1, 3 };
    return values[dir];
}
