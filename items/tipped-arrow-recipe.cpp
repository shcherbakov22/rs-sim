// TippedArrowRecipe: a lingering potion in a ring of 8 arrows

#include "items/recipes.h"

bool tipped_arrow_recipe_matches(const crafting_input & in) {
    if (in.width != 3 || in.height != 3 || in.ingredient_count != 9) {
        return false;
    }
    for (int32_t y = 0; y < in.height; y++) {
        for (int32_t x = 0; x < in.width; x++) {
            item_stack ingredient = crafting_input_get_item(in, x, y);
            if (item_stack_is_empty(ingredient)) {
                return false;
            }
            int32_t expected = x == 1 && y == 1 ? ITEM_LINGERING_POTION : ITEM_ARROW;
            if (ingredient.item != expected) {
                return false;
            }
        }
    }
    return true;
}
