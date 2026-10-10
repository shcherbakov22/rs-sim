// WallTorchBlock

#include "blocks/handlers.h"
#include "engine/level.h"

state_id wall_torch_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    direction facing = (direction) state_get(st, PROP_HORIZONTAL_FACING);
    return direction_opposite(dir) == facing && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

bool wall_torch_can_survive(level & lv, block_pos pos, direction facing) {
    block_pos rel = pos_relative(pos, direction_opposite(facing));
    return block_is_face_sturdy(lv, level_get_state(lv, rel), rel, facing, SUPPORT_TYPE_FULL);
}
