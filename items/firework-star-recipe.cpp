// FireworkStarRecipe: gunpowder and dyes, with at most one shape, twinkle and trail item each

#include "items/recipes.h"

bool firework_star_recipe_matches(const crafting_input & in) {
    if (in.ingredient_count < 2) {
        return false;
    }
    bool gun_powder = false;
    bool colors     = false;
    bool shape      = false;
    bool trail      = false;
    bool twinkle    = false;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) {
            continue;
        }
        if (ingredient_test(recipe_field_ingredients[RECIPE_FIELD_FIREWORK_STAR_RECIPE_SHAPE_BY_ITEM], s)) {
            if (shape) {
                return false;
            }
            shape = true;
        } else if (ingredient_test(recipe_field_ingredients[RECIPE_FIELD_FIREWORK_STAR_RECIPE_TWINKLE_INGREDIENT], s)) {
            if (twinkle) {
                return false;
            }
            twinkle = true;
        } else if (ingredient_test(recipe_field_ingredients[RECIPE_FIELD_FIREWORK_STAR_RECIPE_TRAIL_INGREDIENT], s)) {
            if (trail) {
                return false;
            }
            trail = true;
        } else if (ingredient_test(recipe_field_ingredients[RECIPE_FIELD_FIREWORK_STAR_RECIPE_GUNPOWDER_INGREDIENT],
                                   s)) {
            if (gun_powder) {
                return false;
            }
            gun_powder = true;
        } else {
            if (!item_instanceof(s.item, ICLASS_DYE_ITEM)) {
                return false;
            }
            colors = true;
        }
    }
    return gun_powder && colors;
}
