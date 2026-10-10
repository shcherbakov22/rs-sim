#pragma once

#include "core/common.h"

#include <string_view>

// vanilla Direction, in 3D data value order
enum direction {
    DIRECTION_DOWN,
    DIRECTION_UP,
    DIRECTION_NORTH,
    DIRECTION_SOUTH,
    DIRECTION_WEST,
    DIRECTION_EAST,
};

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
