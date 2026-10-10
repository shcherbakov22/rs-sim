// LeavesBlock: distance to the nearest log, refreshed one tick after a neighbor changes

#include "blocks/handlers.h"
#include "engine/level.h"

#include <algorithm>

// getOptionalDistanceAt, 7 when the state has no distance
static int32_t leaves_get_distance_at(state_id st) {
    if (state_has(st, STATE_FLAG_TAG_LOGS)) {
        return 0;
    }
    return state_has_prop(st, PROP_DISTANCE) ? state_get(st, PROP_DISTANCE) : 7;
}

static state_id leaves_update_distance(level & lv, state_id st, block_pos pos) {
    int32_t new_distance = 7;
    for (int32_t d = 0; d < 6; d++) {
        new_distance =
            std::min(new_distance, leaves_get_distance_at(level_get_state(lv, pos_relative(pos, (direction) d))) + 1);
        if (new_distance == 1) {
            break;
        }
    }
    return state_set(st, PROP_DISTANCE, new_distance);
}

void leaves_tick(level & lv, state_id st, block_pos pos) {
    level_set_block(lv, pos, leaves_update_distance(lv, st, pos), UPDATE_FLAG_ALL);
}

// only a log beside leaves at distance 1 skips the tick
state_id leaves_update_shape(level & lv, state_id st, block_pos pos, state_id neighbor) {
    int32_t distance_from_neighbor = leaves_get_distance_at(neighbor) + 1;
    if (distance_from_neighbor != 1 || state_get(st, PROP_DISTANCE) != distance_from_neighbor) {
        level_schedule_tick(lv, pos, state_block(st), 1);
    }
    return st;
}
