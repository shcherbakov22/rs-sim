// DecoratedPotRecipe: four sherds or bricks in a plus shape

#include "engine/level.h"
#include "items/recipes.h"

static bool decorated_pot_recipe_is_ingredient(const item_stack & s) {
    return (item_defs[item_stack_get_item(s)].tags & ITEM_TAG_DECORATED_POT_INGREDIENTS) != 0;
}

// back, left, right and front
bool decorated_pot_recipe_matches(const crafting_input & in) {
    if (in.width != 3 || in.height != 3 || in.ingredient_count != 4) {
        return false;
    }
    return decorated_pot_recipe_is_ingredient(crafting_input_get_item(in, 1, 0)) &&
           decorated_pot_recipe_is_ingredient(crafting_input_get_item(in, 0, 1)) &&
           decorated_pot_recipe_is_ingredient(crafting_input_get_item(in, 2, 1)) &&
           decorated_pot_recipe_is_ingredient(crafting_input_get_item(in, 1, 2));
}

// PotDecorations stores a brick side as empty, so a pot of four bricks keeps the default PotDecorations.EMPTY
item_stack decorated_pot_recipe_assemble(level & lv, const crafting_input & in) {
    item_stack sides[4] = { crafting_input_get_item(in, 1, 0), crafting_input_get_item(in, 0, 1),
                            crafting_input_get_item(in, 2, 1), crafting_input_get_item(in, 1, 2) };
    for (const item_stack & s : sides) {
        if (s.item != ITEM_BRICK) {
            level_unsupported(lv, "DecoratedPotRecipe.assemble with sherds");
            return {};
        }
    }
    return { ITEM_DECORATED_POT, 1 };
}
