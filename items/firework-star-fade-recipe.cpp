// FireworkStarFadeRecipe: one firework star and dyes

#include "items/recipes.h"

bool firework_star_fade_recipe_matches(const crafting_input & in) {
    if (in.ingredient_count < 2) {
        return false;
    }
    bool color = false;
    bool star  = false;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) {
            continue;
        }
        if (item_instanceof(s.item, ICLASS_DYE_ITEM)) {
            color = true;
        } else {
            if (!ingredient_test(recipe_field_ingredients[RECIPE_FIELD_FIREWORK_STAR_FADE_RECIPE_STAR_INGREDIENT], s)) {
                return false;
            }
            if (star) {
                return false;
            }
            star = true;
        }
    }
    return star && color;
}
