// CandleCakeBlock: an empty hand on the candle (the top face) puts it out

#include "blocks/handlers.h"
#include "engine/level.h"

// only the top face hit point lies above the middle of the block
interaction_result candle_cake_use_item_on(level &            lv,
                                           state_id           st,
                                           block_pos          pos,
                                           direction          face,
                                           const item_stack & stack) {
    if (stack.item == ITEM_FLINT_AND_STEEL || stack.item == ITEM_FIRE_CHARGE) {
        return INTERACTION_RESULT_PASS;
    }
    if (face == DIRECTION_UP && item_stack_is_empty(stack) && state_get(st, PROP_LIT)) {
        abstract_candle_extinguish(lv, st, pos);
        return INTERACTION_RESULT_SUCCESS;
    }
    return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
}

// the candle drops only when the cake is eaten
interaction_result candle_cake_use_without_item() {
    return cake_eat();
}

state_id candle_cake_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    return dir == DIRECTION_DOWN && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

bool candle_cake_can_survive(level & lv, block_pos pos) {
    return state_has(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)), STATE_FLAG_SOLID);
}

// CakeBlock.FULL_CAKE_SIGNAL
int32_t candle_cake_get_analog_output_signal() {
    return cake_get_analog_output_signal(block_defs[BLOCK_CAKE].state_default);
}
