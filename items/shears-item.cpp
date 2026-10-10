// ShearsItem: shears stop a growing plant head at its max age

#include "engine/level.h"
#include "items/handlers.h"

interaction_result shears_item_use_on(level & lv, const use_on_context & hit) {
    state_id st = level_get_state(lv, hit.pos);
    if (!block_instanceof(state_block(st), VCLASS_GROWING_PLANT_HEAD_BLOCK) || state_get(st, PROP_AGE_25) == 25) {
        return INTERACTION_RESULT_PASS;
    }
    level_set_block(lv, hit.pos, state_set(st, PROP_AGE_25, 25), UPDATE_FLAG_ALL);
    return INTERACTION_RESULT_SUCCESS;
}
