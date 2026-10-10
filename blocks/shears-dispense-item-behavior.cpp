// ShearsDispenseItemBehavior: shears a full hive; with no entities nothing else can be sheared

#include "blocks/handlers.h"
#include "engine/level.h"

// dropping the honeycomb is a spawn, which ends the run
item_stack shears_dispense_item_behavior_execute(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, dispenser_facing(st));
    state_id  at     = level_get_state(lv, target);
    if (state_has(at, STATE_FLAG_TAG_BEEHIVES) && block_instanceof(state_block(at), VCLASS_BEEHIVE_BLOCK) &&
        state_get(at, PROP_LEVEL_HONEY) >= 5) {
        beehive_drop_honeycomb(lv, target);
    }
    return dispensed;
}
