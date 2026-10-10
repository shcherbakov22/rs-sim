// MapExtendingRecipe: a filled map in paper zooms out; it needs the map's saved data

#include "engine/level.h"
#include "items/recipes.h"

// a fresh filled map has no MAP_ID, so MapItem.getSavedData finds no data
bool map_extending_recipe_matches(level & lv, int32_t recipe, const crafting_input & in) {
    if (!shaped_recipe_matches(recipe, in)) {
        return false;
    }
    for (const item_stack & s : in.items) {
        if (!item_stack_is_empty(s) && (item_defs[s.item].components & ITEM_COMPONENT_MAP_ID)) {
            level_unsupported(lv, "MapItem.getSavedData");
            return false;
        }
    }
    return false;
}
