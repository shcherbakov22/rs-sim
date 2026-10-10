// BannerDuplicateRecipe: a patterned banner copies onto a blank one of the same color

#include "items/recipes.h"

bool banner_duplicate_recipe_matches(const crafting_input & in) {
    if (in.ingredient_count != 2) {
        return false;
    }
    int32_t color      = -1;
    bool    has_target = false;
    bool    has_source = false;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) {
            continue;
        }
        if (!item_instanceof(s.item, ICLASS_BANNER_ITEM)) {
            return false;
        }
        // BannerItem.getColor
        int32_t banner_color = block_field_get(item_defs[s.item].block, FIELD_ABSTRACT_BANNER_BLOCK_COLOR);
        if (color < 0) {
            color = banner_color;
        } else if (color != banner_color) {
            return false;
        }
        // only the item's default patterns, never more than 6 layers
        if (item_defs[s.item].components & ITEM_COMPONENT_BANNER_PATTERNS) {
            if (has_source) {
                return false;
            }
            has_source = true;
        } else {
            if (has_target) {
                return false;
            }
            has_target = true;
        }
    }
    return has_source && has_target;
}
