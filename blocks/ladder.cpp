// LadderBlock: needs a full sturdy face behind it

#include "blocks/handlers.h"
#include "engine/level.h"

bool ladder_can_survive(level & lv, state_id st, block_pos pos) {
    direction facing = (direction) state_get(st, PROP_HORIZONTAL_FACING);
    block_pos behind = pos_relative(pos, direction_opposite(facing));
    return block_is_face_sturdy(lv, level_get_state(lv, behind), behind, facing, SUPPORT_TYPE_FULL);
}

// a ladder that breaks skips the water tick
state_id ladder_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    if (direction_opposite(dir) == (direction) state_get(st, PROP_HORIZONTAL_FACING) &&
        !block_can_survive(lv, st, pos)) {
        return STATE_AIR;
    }
    if (state_get(st, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
    }
    return st;
}
