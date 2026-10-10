// ShapedRecipe: items in a fixed pattern, which may also be mirrored left to right (ShapedRecipePattern)

#include "items/recipes.h"

// ShapedRecipePattern.matches(CraftingInput, boolean)
static bool shaped_recipe_matches_flip(const recipe_def & def, const crafting_input & in, bool x_flip) {
    for (int32_t y = 0; y < def.height; y++) {
        for (int32_t x = 0; x < def.width; x++) {
            int32_t    column   = x_flip ? def.width - x - 1 : x;
            int32_t    expected = recipe_ingredients[def.first + column + y * def.width];
            item_stack actual   = crafting_input_get_item(in, x, y);
            // Ingredient.testOptionalIngredient
            bool       ok       = expected < 0 ? item_stack_is_empty(actual) : ingredient_test(expected, actual);
            if (!ok) {
                return false;
            }
        }
    }
    return true;
}

// ShapedRecipePattern.matches; a symmetrical pattern only skips a mirrored test that gives the same answer
bool shaped_recipe_matches(int32_t recipe, const crafting_input & in) {
    const recipe_def & def              = recipe_defs[recipe];
    int32_t            ingredient_count = 0;
    for (int32_t i = 0; i < def.n_ingredients; i++) {
        ingredient_count += recipe_ingredients[def.first + i] < 0 ? 0 : 1;
    }
    if (in.ingredient_count != ingredient_count || in.width != def.width || in.height != def.height) {
        return false;
    }
    return shaped_recipe_matches_flip(def, in, true) || shaped_recipe_matches_flip(def, in, false);
}
