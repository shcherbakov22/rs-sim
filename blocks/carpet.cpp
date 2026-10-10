// CarpetBlock: any shape update removes a carpet that has air below

#include "blocks/handlers.h"
#include "engine/level.h"

state_id carpet_update_shape(level & lv, state_id st, block_pos pos) {
    return !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

bool carpet_can_survive(level & lv, block_pos pos) {
    return !state_has(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)), STATE_FLAG_AIR);
}
