// ShovelItem: flattens dirt-like blocks into a path when the block above is air, or puts out a lit campfire

#include "engine/level.h"
#include "items/handlers.h"

interaction_result shovel_item_use_on(level & lv, const use_on_context & hit) {
    if (hit.face == DIRECTION_DOWN) {
        return INTERACTION_RESULT_PASS;
    }
    state_id st        = level_get_state(lv, hit.pos);
    int32_t  flattened = block_defs[state_block(st)].flattened;
    if (flattened >= 0 && state_has(level_get_state(lv, pos_relative(hit.pos, DIRECTION_UP)), STATE_FLAG_AIR)) {
        level_set_block(lv, hit.pos, block_default(flattened), UPDATE_FLAG_ALL_IMMEDIATE);
        return INTERACTION_RESULT_SUCCESS;
    }
    if (block_instanceof(state_block(st), VCLASS_CAMPFIRE_BLOCK) && state_get(st, PROP_LIT)) {
        level_unsupported(lv, "CampfireBlock.dowse");
    }
    return INTERACTION_RESULT_PASS;
}
