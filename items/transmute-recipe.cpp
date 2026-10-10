// TransmuteRecipe: an input item and one material give the result item, keeping the input's data components

#include "engine/level.h"
#include "items/recipes.h"

static int32_t transmute_recipe_input(const recipe_def & def) {
    return recipe_ingredients[def.first];
}

static int32_t transmute_recipe_material(const recipe_def & def) {
    return recipe_ingredients[def.first + 1];
}

// TransmuteResult.isResultUnchanged: the input's data components carry over, so with no added components only the item and count differ
static bool transmute_recipe_is_result_unchanged(level & lv, const recipe_def & def, const item_stack & input) {
    if (def.result_components) {
        level_unsupported(lv, "recipe %s: result data components", def.name);
    }
    return def.count == 1 && def.result == item_stack_get_item(input);
}

bool transmute_recipe_matches(level & lv, int32_t recipe, const crafting_input & in) {
    const recipe_def & def = recipe_defs[recipe];
    if (in.ingredient_count != 2) {
        return false;
    }
    bool found_input    = false;
    bool found_material = false;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) {
            continue;
        }
        if (!found_input && ingredient_test(transmute_recipe_input(def), s)) {
            if (transmute_recipe_is_result_unchanged(lv, def, s)) {
                return false;
            }
            found_input = true;
        } else {
            if (found_material || !ingredient_test(transmute_recipe_material(def), s)) {
                return false;
            }
            found_material = true;
        }
    }
    return found_input && found_material;
}

// TransmuteResult.apply on a stack with only default data components
item_stack transmute_recipe_assemble(int32_t recipe, const crafting_input & in) {
    const recipe_def & def = recipe_defs[recipe];
    for (const item_stack & s : in.items) {
        if (!item_stack_is_empty(s) && ingredient_test(transmute_recipe_input(def), s)) {
            return { def.result, def.count };
        }
    }
    return {};
}
