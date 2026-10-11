#pragma once

#include "core/common.h"
#include "gen/blocks.h"

#include <cfloat>

static_assert(DIRECTION_DOWN == 0 && DIRECTION_EAST == 5);

// Direction.Plane.HORIZONTAL and VERTICAL
inline constexpr direction DIRECTION_HORIZONTAL[4] = { DIRECTION_NORTH, DIRECTION_EAST, DIRECTION_SOUTH,
                                                       DIRECTION_WEST };
inline constexpr direction DIRECTION_VERTICAL[2]   = { DIRECTION_UP, DIRECTION_DOWN };

// Direction.getNormal
inline constexpr block_pos DIRECTION_STEPS[6] = {
    { 0,  -1, 0  },
    { 0,  1,  0  },
    { 0,  0,  -1 },
    { 0,  0,  1  },
    { -1, 0,  0  },
    { 1,  0,  0  }
};

inline direction direction_opposite(direction dir) { return (direction) (dir ^ 1); }

// Direction.getApproximateNearest: the largest dot product in float, NORTH when none is positive
inline direction direction_get_approximate_nearest(double x, double y, double z) {
    direction result  = DIRECTION_NORTH;
    float     highest = FLT_TRUE_MIN;  // Float.MIN_VALUE
    for (int32_t i = 0; i < 6; i++) {
        const block_pos & s   = DIRECTION_STEPS[i];
        float             dot = (float) x * s.x + (float) y * s.y + (float) z * s.z;
        if (dot > highest) {
            highest = dot;
            result  = (direction) i;
        }
    }
    return result;
}

inline block_pos pos_relative(block_pos pos, direction dir) {
    const block_pos & s = DIRECTION_STEPS[dir];
    return { pos.x + s.x, pos.y + s.y, pos.z + s.z };
}

// BlockPos.relative(Direction, int)
inline block_pos pos_relative_steps(block_pos pos, direction dir, int32_t steps) {
    const block_pos & s = DIRECTION_STEPS[dir];
    return { pos.x + s.x * steps, pos.y + s.y * steps, pos.z + s.z * steps };
}

inline bool direction_is_horizontal(direction dir) { return dir >= DIRECTION_NORTH; }

// Direction.getAxisDirection().getStep()
inline int32_t direction_get_axis_step(direction dir) {
    const block_pos & s = DIRECTION_STEPS[dir];
    return s.x + s.y + s.z;
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

inline direction direction_counter_clockwise(direction dir) { return direction_opposite(direction_clockwise(dir)); }
