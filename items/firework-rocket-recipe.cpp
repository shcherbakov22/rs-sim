// FireworkRocketRecipe: paper, 1..3 gunpowder and any firework stars

#include "items/recipes.h"

bool firework_rocket_recipe_matches(const crafting_input & in) {
    if (in.ingredient_count < 2) {
        return false;
    }
    bool    paper      = false;
    int32_t gun_powder = 0;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) {
            continue;
        }
        if (ingredient_test(recipe_field_ingredients[RECIPE_FIELD_FIREWORK_ROCKET_RECIPE_PAPER_INGREDIENT], s)) {
            if (paper) {
                return false;
            }
            paper = true;
        } else if (ingredient_test(recipe_field_ingredients[RECIPE_FIELD_FIREWORK_ROCKET_RECIPE_GUNPOWDER_INGREDIENT],
                                   s)) {
            if (++gun_powder > 3) {
                return false;
            }
        } else if (!ingredient_test(recipe_field_ingredients[RECIPE_FIELD_FIREWORK_ROCKET_RECIPE_STAR_INGREDIENT], s)) {
            return false;
        }
    }
    return paper && gun_powder >= 1;
}
