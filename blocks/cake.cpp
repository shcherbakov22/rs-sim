// CakeBlock: a candle on an uneaten cake makes the matching candle cake

#include "blocks/handlers.h"
#include "engine/level.h"

// CandleCakeBlock.byCandle
static state_id cake_candle_cake_by_candle(int32_t candle) {
    for (int32_t block = 0; block < BLOCK_COUNT; block++) {
        if (block_defs[block].cls == VCLASS_CANDLE_CAKE_BLOCK &&
            block_field_get(block, FIELD_CANDLE_CAKE_BLOCK_CANDLE_BLOCK) == candle) {
            return block_defs[block].state_default;
        }
    }
    TMC_ASSERT(false);
    return 0;
}

interaction_result cake_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack) {
    int32_t candle = item_defs[stack.item].block;
    if ((item_defs[stack.item].tags & ITEM_TAG_CANDLES) && state_get(st, PROP_BITES) == 0 && candle >= 0 &&
        block_defs[candle].cls == VCLASS_CANDLE_BLOCK) {
        level_set_block(lv, pos, cake_candle_cake_by_candle(candle), UPDATE_FLAG_ALL);
        return INTERACTION_RESULT_SUCCESS;
    }
    return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
}

// the virtual player is never hungry, so it can not eat
interaction_result cake_eat() {
    return INTERACTION_RESULT_PASS;
}

state_id cake_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    return dir == DIRECTION_DOWN && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

bool cake_can_survive(level & lv, block_pos pos) {
    return state_has(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)), STATE_FLAG_SOLID);
}

int32_t cake_get_analog_output_signal(state_id st) {
    return (7 - state_get(st, PROP_BITES)) * 2;
}
