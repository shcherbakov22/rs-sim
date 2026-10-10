// HoneycombItem: waxes copper; the other half of a double chest follows by its own shape update

#include "engine/level.h"
#include "items/handlers.h"

interaction_result honeycomb_item_use_on(level & lv, const use_on_context & hit) {
    state_id st    = level_get_state(lv, hit.pos);
    int32_t  waxed = block_defs[state_block(st)].waxed;
    if (waxed < 0) {
        return INTERACTION_RESULT_PASS;
    }
    level_set_block(lv, hit.pos, block_with_properties_of(waxed, st), UPDATE_FLAG_ALL_IMMEDIATE);
    return INTERACTION_RESULT_SUCCESS;
}
