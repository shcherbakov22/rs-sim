// ArmorDyeRecipe: one dyeable item and dyes

#include "items/recipes.h"

bool armor_dye_recipe_matches(const crafting_input & in) {
    if (in.ingredient_count < 2) {
        return false;
    }
    bool has_armor = false;
    bool has_dyes  = false;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) {
            continue;
        }
        if (item_defs[s.item].tags & ITEM_TAG_DYEABLE) {
            if (has_armor) {
                return false;
            }
            has_armor = true;
        } else {
            if (!item_instanceof(s.item, ICLASS_DYE_ITEM)) {
                return false;
            }
            has_dyes = true;
        }
    }
    return has_dyes && has_armor;
}
