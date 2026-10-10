// EndCrystalItem: an end crystal goes on obsidian or bedrock with air above; there is no dragon fight outside the End

#include "core/trace.h"
#include "engine/level.h"
#include "items/handlers.h"

interaction_result end_crystal_item_use_on(level & lv, const use_on_context & hit) {
    state_id st = level_get_state(lv, hit.pos);
    if (!state_is(st, BLOCK_OBSIDIAN) && !state_is(st, BLOCK_BEDROCK)) {
        return INTERACTION_RESULT_FAIL;
    }
    block_pos above = pos_relative(hit.pos, DIRECTION_UP);
    if (!state_has(level_get_state(lv, above), STATE_FLAG_AIR)) {
        return INTERACTION_RESULT_FAIL;
    }
    trace_boundary(lv.tr, "spawn end_crystal %d %d %d", above.x, above.y, above.z);
    return INTERACTION_RESULT_SUCCESS;
}
