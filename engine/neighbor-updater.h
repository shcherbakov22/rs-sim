#pragma once

#include "core/common.h"
#include "core/direction.h"
#include "core/state.h"

#include <vector>

// CollectingNeighborUpdater. Updates queued while one runs go on a stack: depth first, in queue order.

enum neighbor_update_type {
    NEIGHBOR_UPDATE_SHAPE,
    NEIGHBOR_UPDATE_SIMPLE,
    NEIGHBOR_UPDATE_FULL,
    NEIGHBOR_UPDATE_MULTI,
};

struct neighbor_update {
    neighbor_update_type type            = {};
    block_pos            pos             = {};  // the updated block (multi: the source)
    block_pos            neighbor_pos    = {};  // shape
    state_id             state           = {};  // shape: neighbor state, full: state to update
    int32_t              block           = {};  // simple, full, multi: the changed block
    direction            dir             = {};  // shape
    int32_t              skip            = {};  // multi: direction to skip, -1 for none
    int32_t              idx             = {};  // multi: next index in NEIGHBOR_UPDATE_ORDER
    int32_t              flags           = {};  // shape
    int32_t              limit           = {};  // shape
    bool                 moved_by_piston = {};  // full
};

struct neighbor_updater {
    std::vector<neighbor_update> stack;  // top at the back
    std::vector<neighbor_update> added_this_layer;
    int32_t                      count = 0;
    int64_t                      run   = 0;  // updates run since the level started: what lag a build causes
};

// NeighborUpdater.UPDATE_ORDER
inline constexpr direction NEIGHBOR_UPDATE_ORDER[6] = {
    DIRECTION_WEST, DIRECTION_EAST, DIRECTION_DOWN, DIRECTION_UP, DIRECTION_NORTH, DIRECTION_SOUTH,
};

struct level;

void neighbor_updater_add(level & lv, const neighbor_update & update);
