// AbstractCauldronBlock: the item in hand picks an interaction from the cauldron's CauldronInteraction map

#include "blocks/handlers.h"
#include "engine/level.h"

// what the filled bottle or bucket does is the player's; only the cauldron change is in the trace
interaction_result abstract_cauldron_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack) {
    int32_t map  = block_defs[state_block(st)].cauldron_map;
    int32_t item = item_stack_get_item(stack);
    if ((item_defs[item].cauldron_maps & (1u << map)) == 0) {
        return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
    }
    if (map == CAULDRON_MAP_WATER && item == ITEM_GLASS_BOTTLE) {
        layered_cauldron_lower_fill_level(lv, st, pos);
        return INTERACTION_RESULT_SUCCESS;
    }
    level_unsupported(lv, "CauldronInteraction %s %s", cauldron_map_names[map], item_defs[item].name);
    return INTERACTION_RESULT_PASS;
}
