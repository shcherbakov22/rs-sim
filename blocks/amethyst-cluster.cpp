// AmethystClusterBlock

#include "blocks/handlers.h"
#include "engine/level.h"

bool amethyst_cluster_can_survive(level & lv, state_id st, block_pos pos) {
    direction dir          = (direction) state_get(st, PROP_FACING);
    block_pos adjacent_pos = pos_relative(pos, direction_opposite(dir));
    return block_is_face_sturdy(lv, level_get_state(lv, adjacent_pos), adjacent_pos, dir, SUPPORT_TYPE_FULL);
}

state_id amethyst_cluster_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    if (dir == direction_opposite((direction) state_get(st, PROP_FACING)) &&
        !amethyst_cluster_can_survive(lv, st, pos)) {
        return STATE_AIR;
    }
    return st;
}
