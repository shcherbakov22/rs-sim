// WallTorchBlock

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

state_id wall_torch_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    direction facing = (direction) state_get(st, PROP_HORIZONTAL_FACING);
    return direction_opposite(dir) == facing && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

bool wall_torch_can_survive(level & lv, block_pos pos, direction facing) {
    block_pos rel = pos_relative(pos, direction_opposite(facing));
    return block_is_face_sturdy(lv, level_get_state(lv, rel), rel, facing, SUPPORT_TYPE_FULL);
}

// the first horizontal looking direction with a wall to hang on
state_id wall_torch_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    block_pos pos = block_place_context_get_clicked_pos(ctx);
    for (direction dir : block_place_context_get_nearest_looking_directions(ctx)) {
        if (direction_is_horizontal(dir)) {
            state_id st = state_set(block_default(block), PROP_HORIZONTAL_FACING, direction_opposite(dir));
            if (block_can_survive(lv, st, pos)) {
                return st;
            }
        }
    }
    return STATE_NONE;
}
