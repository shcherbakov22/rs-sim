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

// ItemStack.listMatches
static bool item_stack_list_matches(const std::vector<item_stack> & a, const std::vector<item_stack> & b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (item_stack_get_count(a[i]) != item_stack_get_count(b[i]) ||
            !item_stack_is_same_item_same_components(a[i], b[i])) {
            return false;
        }
    }
    return true;
}

int32_t item_container_contents_from_items(level & lv, const std::vector<item_stack> & items) {
    int32_t last = (int32_t) items.size() - 1;
    while (last >= 0 && item_stack_is_empty(items[last])) last--;
    if (last < 0) return -1;
    std::vector<item_stack> copy;
    for (int32_t i = 0; i <= last; i++) {
        copy.push_back(item_stack_copy_with_count(items[i], item_stack_get_count(items[i])));
    }
    for (size_t k = 0; k < lv.item_contents.size(); k++) {
        if (item_stack_list_matches(lv.item_contents[k], copy)) return (int32_t) k;
    }
    lv.item_contents.push_back(copy);
    return (int32_t) lv.item_contents.size() - 1;
}

void item_container_contents_copy_into(const level & lv, int32_t contents, std::vector<item_stack> & destination) {
    for (size_t i = 0; i < destination.size(); i++) {
        bool held      = contents >= 0 && i < lv.item_contents[contents].size();
        destination[i] = held ? lv.item_contents[contents][i] : item_stack();
    }
}

// BoneMealItem: bonemealing plants and growing seagrass in water are out of scope, so reaching either stops the run
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
