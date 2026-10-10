// LanternBlock: hangs from the center of the block above, or stands on the center of the block below

#include "blocks/handlers.h"
#include "engine/level.h"

// getConnectedDirection, opposite: the side the support is on
static direction lantern_support_direction(state_id st) {
    return state_get(st, PROP_HANGING) ? DIRECTION_UP : DIRECTION_DOWN;
}

bool lantern_can_survive(level & lv, state_id st, block_pos pos) {
    direction dir = lantern_support_direction(st);
    return block_can_support_center(lv, pos_relative(pos, dir), direction_opposite(dir));
}

state_id lantern_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    return lantern_support_direction(st) == dir && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}
