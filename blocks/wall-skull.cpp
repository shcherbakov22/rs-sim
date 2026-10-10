// WallSkullBlock: a skull on the side of a block

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

// the first horizontal looking direction whose block cannot be replaced holds the skull
state_id wall_skull_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    state_id  st  = abstract_skull_get_state_for_placement(lv, block, ctx);
    block_pos pos = block_place_context_get_clicked_pos(ctx);
    for (direction dir : block_place_context_get_nearest_looking_directions(ctx)) {
        if (direction_is_horizontal(dir)) {
            st           = state_set(st, PROP_HORIZONTAL_FACING, direction_opposite(dir));
            block_pos at = pos_relative(pos, dir);
            if (!block_can_be_replaced(lv, level_get_state(lv, at), ctx)) {
                return st;
            }
        }
    }
    return STATE_NONE;
}
