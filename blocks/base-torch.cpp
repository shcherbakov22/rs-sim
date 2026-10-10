// BaseTorchBlock

#include "blocks/handlers.h"
#include "engine/level.h"

state_id base_torch_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    return dir == DIRECTION_DOWN && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

// Block.canSupportCenter; UNSTABLE_BOTTOM_CENTER only matters for a downward face
bool base_torch_can_survive(level & lv, block_pos pos) {
    return block_is_face_sturdy(lv, level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)), DIRECTION_UP,
                                SUPPORT_TYPE_CENTER);
}
