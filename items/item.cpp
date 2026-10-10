// Item behaviour: the item hooks the sim dispatches, and BoneMealItem for dispensers

#include "engine/level.h"
#include "items/handlers.h"

// ItemStack.onCraftedBySystem; no ported item class overrides onCraftedPostProcess, so any override stops the run
void item_on_crafted_post_process(level & lv, const item_stack & s) {
    int16_t owner = iclass_hooks[item_defs[s.item].cls][ITEM_HOOK_ON_CRAFTED_POST_PROCESS];
    if (owner != ICLASS_NONE) {
        level_unsupported(lv, "%s.%s", iclass_names[owner], item_hook_names[ITEM_HOOK_ON_CRAFTED_POST_PROCESS]);
    }
}

// BoneMealItem: bonemealing plants and growing seagrass in water are out of scope, so reaching either stops the run

// isValidBonemealTarget is not ported, so every bonemealable block stops the run
bool bone_meal_item_grow_crop(level & lv, block_pos pos) {
    state_id st = level_get_state(lv, pos);
    if (!state_has(st, STATE_FLAG_BONEMEALABLE)) return false;
    level_unsupported(lv, "bone meal on %s", block_defs[state_block(st)].name);
    return true;
}

// flowing water holds less than a full source and makes the growth fail; that case stops the run too
bool bone_meal_item_grow_water_plant(level & lv, block_pos pos) {
    if (!state_is(level_get_state(lv, pos), BLOCK_WATER)) return false;
    level_unsupported(lv, "fluids");
    return true;
}
