// DecoratedPotBlock and DecoratedPotBlockEntity: a one-slot container that wobbles (block event 1) when used

#include "blocks/handlers.h"
#include "engine/level.h"

static block_entity * decorated_pot_block_entity(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    return be != nullptr && be->type == BLOCK_ENTITY_TYPE_DECORATED_POT ? be : nullptr;
}

// the pot takes one item from the stack while its own stack has room by the item's own limit
interaction_result decorated_pot_use_item_on(level & lv, block_pos pos, const item_stack & stack) {
    block_entity * be = decorated_pot_block_entity(lv, pos);
    if (be == nullptr) {
        return INTERACTION_RESULT_PASS;
    }
    item_stack & pot_item = be->items[0];
    bool         fits =
        item_stack_is_empty(pot_item) || (item_stack_get_item(pot_item) == item_stack_get_item(stack) &&
                                          item_stack_get_count(pot_item) < item_stack_get_max_stack_size(pot_item));
    if (item_stack_is_empty(stack) || !fits) {
        return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
    }
    level_block_event(lv, pos, BLOCK_DECORATED_POT, 1, 0);
    if (item_stack_is_empty(pot_item)) {
        pot_item = { stack.item, 1 };
    } else {
        item_stack_grow(pot_item, 1);
    }
    block_entity_set_changed(lv, pos, be->state);
    return INTERACTION_RESULT_SUCCESS;
}

interaction_result decorated_pot_use_without_item(level & lv, block_pos pos) {
    if (decorated_pot_block_entity(lv, pos) == nullptr) {
        return INTERACTION_RESULT_PASS;
    }
    level_block_event(lv, pos, BLOCK_DECORATED_POT, 1, 1);
    return INTERACTION_RESULT_SUCCESS;
}
