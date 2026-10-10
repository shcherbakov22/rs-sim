// ShapelessRecipe: each ingredient takes a different stack of the grid, in any layout

#include "items/recipes.h"

// one augmenting path of StackedContents.tryPick, which finds a full matching whenever one exists
static bool shapeless_recipe_try_assign(const recipe_def &     def,
                                        const crafting_input & in,
                                        int32_t                ingredient,
                                        std::vector<bool> &    visited,
                                        std::vector<int32_t> & slot_owner) {
    for (size_t slot = 0; slot < in.items.size(); slot++) {
        if (visited[slot] || !ingredient_test(recipe_ingredients[def.first + ingredient], in.items[slot])) {
            continue;
        }
        visited[slot] = true;
        if (slot_owner[slot] < 0 || shapeless_recipe_try_assign(def, in, slot_owner[slot], visited, slot_owner)) {
            slot_owner[slot] = ingredient;
            return true;
        }
    }
    return false;
}

// StackedItemContents.canCraft, each non-empty slot counting once
static bool shapeless_recipe_can_craft(const recipe_def & def, const crafting_input & in) {
    std::vector<int32_t> slot_owner(in.items.size(), -1);
    for (int32_t ingredient = 0; ingredient < def.n_ingredients; ingredient++) {
        std::vector<bool> visited(in.items.size(), false);
        if (!shapeless_recipe_try_assign(def, in, ingredient, visited, slot_owner)) {
            return false;
        }
    }
    return true;
}

bool shapeless_recipe_matches(int32_t recipe, const crafting_input & in) {
    const recipe_def & def = recipe_defs[recipe];
    if (in.ingredient_count != def.n_ingredients) {
        return false;
    }
    if (in.items.size() == 1 && def.n_ingredients == 1) {
        return ingredient_test(recipe_ingredients[def.first], in.items[0]);
    }
    return shapeless_recipe_can_craft(def, in);
}
