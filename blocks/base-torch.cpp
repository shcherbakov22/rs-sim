// BaseTorchBlock

#include "blocks/handlers.h"
#include "engine/level.h"

state_id base_torch_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    return dir == DIRECTION_DOWN && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

bool base_torch_can_survive(level & lv, block_pos pos) {
    return block_can_support_center(lv, pos_relative(pos, DIRECTION_DOWN), DIRECTION_UP);
}
