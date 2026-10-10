// BeehiveBlock and BeehiveBlockEntity. Bees are entities, so cases hold only empty hives: an empty hive ticks, burns and
// smokes without effect, and harvesting it only resets the honey level.

#include "blocks/handlers.h"
#include "engine/level.h"

int32_t beehive_get_analog_output_signal(state_id st) {
    return state_get(st, PROP_LEVEL_HONEY);
}

interaction_result beehive_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack) {
    if (state_get(st, PROP_LEVEL_HONEY) < 5 || (stack.item != ITEM_SHEARS && stack.item != ITEM_GLASS_BOTTLE)) {
        return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
    }
    if (stack.item == ITEM_SHEARS) {
        beehive_drop_honeycomb(lv, pos);
    }
    beehive_reset_honey_level(lv, st, pos);
    return INTERACTION_RESULT_SUCCESS;
}

// HARVEST_BEEHIVE is one constant pool of 3 honeycomb, so the loot draws nothing
void beehive_drop_honeycomb(level & lv, block_pos pos) {
    level_pop_resource(lv, pos);
}

// releaseBeesAndResetHoneyLevel also releases the bees, and the hives hold none
void beehive_reset_honey_level(level & lv, state_id st, block_pos pos) {
    level_set_block(lv, pos, state_set(st, PROP_LEVEL_HONEY, 0), UPDATE_FLAG_ALL);
}
