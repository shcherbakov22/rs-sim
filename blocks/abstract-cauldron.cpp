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

// PointedDripstoneBlock.canDrip: a dry tip that hangs down
static bool abstract_cauldron_can_drip(state_id st) {
    return state_is(st, BLOCK_POINTED_DRIPSTONE) && state_get(st, PROP_VERTICAL_DIRECTION) == DIRECTION_DOWN &&
           state_get(st, PROP_DRIPSTONE_THICKNESS) == DRIPSTONE_THICKNESS_TIP && !state_get(st, PROP_WATERLOGGED);
}

// PointedDripstoneBlock.findStalactiteTipAboveCauldron looks 10 blocks up. Dripstone is out of scope, so any tip in that range stops the run, even one behind a block.
void abstract_cauldron_tick(level & lv, block_pos pos) {
    for (int32_t i = 1; i < 11; i++) {
        if (abstract_cauldron_can_drip(level_get_state(lv, { pos.x, pos.y + i, pos.z }))) {
            level_unsupported(lv, "pointed dripstone");
            return;
        }
    }
}
