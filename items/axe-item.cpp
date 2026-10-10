// AxeItem: strips logs, else scrapes one weathering stage off copper, else takes the wax off

#include "engine/level.h"
#include "items/handlers.h"

// the virtual player holds no shield in the off hand, so it never means to block
interaction_result axe_item_use_on(level & lv, const use_on_context & hit) {
    state_id          st = level_get_state(lv, hit.pos);
    const block_def & b  = block_defs[state_block(st)];
    state_id          next;
    if (b.stripped >= 0) {
        next = state_set(block_default(b.stripped), PROP_AXIS, state_get(st, PROP_AXIS));
    } else if (b.weathering_previous >= 0) {
        next = block_with_properties_of(b.weathering_previous, st);
    } else if (b.wax_off >= 0) {
        next = block_with_properties_of(b.wax_off, st);
    } else {
        return INTERACTION_RESULT_PASS;
    }
    level_set_block(lv, hit.pos, next, UPDATE_FLAG_ALL_IMMEDIATE);
    return INTERACTION_RESULT_SUCCESS;
}
