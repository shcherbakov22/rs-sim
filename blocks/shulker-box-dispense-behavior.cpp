// ShulkerBoxDispenseBehavior: places the box in front, facing the dispenser direction over air and up over a block

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/handlers.h"

item_stack shulker_box_dispense_behavior_execute(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    direction facing = dispenser_facing(st);
    block_pos front  = pos_relative(pos, facing);
    direction clicked =
        state_has(level_get_state(lv, pos_relative(front, DIRECTION_DOWN)), STATE_FLAG_AIR) ? facing : DIRECTION_UP;
    // BlockItem.place uses up one item when it places
    if (block_item_place(lv, directional_place_context_init(lv, front, clicked, dispensed)) ==
        INTERACTION_RESULT_SUCCESS) {
        item_stack_grow(dispensed, -1);
    }
    return dispensed;
}
