// ShieldDecorationRecipe: a banner's pattern onto a shield without one

#include "items/recipes.h"

bool shield_decoration_recipe_matches(const crafting_input & in) {
    if (in.ingredient_count != 2) {
        return false;
    }
    bool has_clear_shield   = false;
    bool has_pattern_banner = false;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) {
            continue;
        }
        if (item_instanceof(s.item, ICLASS_BANNER_ITEM)) {
            if (has_pattern_banner) {
                return false;
            }
            has_pattern_banner = true;
        } else {
            if (s.item != ITEM_SHIELD || has_clear_shield ||
                (item_defs[s.item].components & ITEM_COMPONENT_BANNER_PATTERNS)) {
                return false;
            }
            has_clear_shield = true;
        }
    }
    return has_clear_shield && has_pattern_banner;
}
