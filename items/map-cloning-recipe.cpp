// MapCloningRecipe: a filled map copies into empty maps

#include "items/recipes.h"

bool map_cloning_recipe_matches(const crafting_input & in) {
    if (in.ingredient_count < 2) {
        return false;
    }
    bool has_empty_maps = false;
    bool has_source_map = false;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) {
            continue;
        }
        if (item_defs[s.item].components & ITEM_COMPONENT_MAP_ID) {
            if (has_source_map) {
                return false;
            }
            has_source_map = true;
        } else {
            if (s.item != ITEM_MAP) {
                return false;
            }
            has_empty_maps = true;
        }
    }
    return has_source_map && has_empty_maps;
}
