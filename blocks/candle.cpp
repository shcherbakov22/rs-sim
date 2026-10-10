// CandleBlock: an empty hand puts out a lit candle; the virtual player may always build

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

interaction_result candle_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack) {
    if (item_stack_is_empty(stack) && state_get(st, PROP_LIT)) {
        abstract_candle_extinguish(lv, st, pos);
        return INTERACTION_RESULT_SUCCESS;
    }
    return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
}

// a candle placed into a candle adds one
state_id candle_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    state_id st = level_get_state(lv, block_place_context_get_clicked_pos(ctx));
    if (state_is(st, block)) {
        return state_cycle(st, PROP_CANDLES);
    }
    return state_set(block_default(block), PROP_WATERLOGGED, block_place_context_is_water_source(lv, ctx));
}

bool candle_can_be_replaced(state_id st, const block_place_context & ctx) {
    if (item_stack_get_item(ctx.hit.stack) == block_defs[state_block(st)].as_item && state_get(st, PROP_CANDLES) < 4) {
        return true;
    }
    return block_behaviour_can_be_replaced(st, ctx);
}

// only placement asks: a candle stays when its support goes
bool candle_can_survive(level & lv, block_pos pos) {
    return block_can_support_center(lv, pos_relative(pos, DIRECTION_DOWN), DIRECTION_UP);
}
