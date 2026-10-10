// CandleBlock: an empty hand puts out a lit candle; the virtual player may always build

#include "blocks/handlers.h"
#include "engine/level.h"

interaction_result candle_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack) {
    if (item_stack_is_empty(stack) && state_get(st, PROP_LIT)) {
        abstract_candle_extinguish(lv, st, pos);
        return INTERACTION_RESULT_SUCCESS;
    }
    return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
}
