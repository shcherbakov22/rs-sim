// BoatDispenseItemBehavior: a boat needs water in front, or air in front over water; else the item drops

#include "blocks/handlers.h"
#include "engine/level.h"

item_stack boat_dispense_item_behavior_execute(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos front = pos_relative(pos, dispenser_facing(st));
    state_id  at    = level_get_state(lv, front);
    if (state_has(at, STATE_FLAG_FLUID) ||
        (state_has(at, STATE_FLAG_AIR) &&
         state_has(level_get_state(lv, pos_relative(front, DIRECTION_DOWN)), STATE_FLAG_FLUID))) {
        level_unsupported(lv, "fluids");
        return dispensed;
    }
    return default_dispense_item_behavior_dispense(lv, st, pos, dispensed);
}
