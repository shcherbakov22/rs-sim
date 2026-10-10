// RecipeManager and Recipe: finds the crafting recipe for a grid and crafts it

#include "engine/level.h"
#include "items/recipes.h"

// CraftingInput.ofPositioned
crafting_input crafting_input_of(int32_t width, int32_t height, const item_stack * items) {
    int32_t left   = width - 1;
    int32_t right  = 0;
    int32_t top    = height - 1;
    int32_t bottom = 0;
    for (int32_t y = 0; y < height; y++) {
        bool row_empty = true;
        for (int32_t x = 0; x < width; x++) {
            if (!item_stack_is_empty(items[x + y * width])) {
                left      = std::min(left, x);
                right     = std::max(right, x);
                row_empty = false;
            }
        }
        if (!row_empty) {
            top    = std::min(top, y);
            bottom = std::max(bottom, y);
        }
    }
    crafting_input in = { 0, 0, {}, 0 };
    if (right - left + 1 <= 0 || bottom - top + 1 <= 0) {
        return in;
    }
    in.width  = right - left + 1;
    in.height = bottom - top + 1;
    for (int32_t y = 0; y < in.height; y++) {
        for (int32_t x = 0; x < in.width; x++) {
            const item_stack & s = items[x + left + (y + top) * width];
            in.items.push_back(s);
            in.ingredient_count += item_stack_is_empty(s) ? 0 : 1;
        }
    }
    return in;
}

item_stack crafting_input_get_item(const crafting_input & in, int32_t x, int32_t y) {
    return in.items[x + y * in.width];
}

// Ingredient.test: the empty stack is air, which no ingredient holds
bool ingredient_test(int32_t ingredient, const item_stack & s) {
    int32_t                item = item_stack_get_item(s);
    const ingredient_def & def  = ingredient_defs[ingredient];
    for (int32_t i = 0; i < def.count; i++) {
        if (ingredient_items[def.first + i] == item) {
            return true;
        }
    }
    return false;
}

static bool recipe_matches(level & lv, int32_t recipe, const crafting_input & in) {
    switch (recipe_defs[recipe].cls) {
        case RCLASS_SHAPED_RECIPE:
            return shaped_recipe_matches(recipe, in);
        case RCLASS_SHAPELESS_RECIPE:
            return shapeless_recipe_matches(recipe, in);
        case RCLASS_TRANSMUTE_RECIPE:
            return transmute_recipe_matches(lv, recipe, in);
        case RCLASS_MAP_EXTENDING_RECIPE:
            return map_extending_recipe_matches(lv, recipe, in);
        case RCLASS_ARMOR_DYE_RECIPE:
            return armor_dye_recipe_matches(in);
        case RCLASS_BANNER_DUPLICATE_RECIPE:
            return banner_duplicate_recipe_matches(in);
        case RCLASS_BOOK_CLONING_RECIPE:
            return book_cloning_recipe_matches(in);
        case RCLASS_DECORATED_POT_RECIPE:
            return decorated_pot_recipe_matches(in);
        case RCLASS_FIREWORK_ROCKET_RECIPE:
            return firework_rocket_recipe_matches(in);
        case RCLASS_FIREWORK_STAR_FADE_RECIPE:
            return firework_star_fade_recipe_matches(in);
        case RCLASS_FIREWORK_STAR_RECIPE:
            return firework_star_recipe_matches(in);
        case RCLASS_MAP_CLONING_RECIPE:
            return map_cloning_recipe_matches(in);
        case RCLASS_REPAIR_ITEM_RECIPE:
            return repair_item_recipe_matches(in);
        case RCLASS_SHIELD_DECORATION_RECIPE:
            return shield_decoration_recipe_matches(in);
        case RCLASS_TIPPED_ARROW_RECIPE:
            return tipped_arrow_recipe_matches(in);
    }
    TMC_ASSERT(false);
    return false;
}

// CrafterBlock.RECIPE_CACHE only remembers earlier answers, so it is left out
int32_t recipe_manager_get_recipe_for(level & lv, const crafting_input & in) {
    if (in.ingredient_count == 0) {
        return -1;
    }
    for (int32_t r = 0; r < n_recipe_defs; r++) {
        if (recipe_matches(lv, r, in)) {
            return r;
        }
        if (!lv.unsupported.empty()) {
            return -1;
        }
    }
    return -1;
}

// most special recipes build their results from data components the sim does not keep
item_stack recipe_assemble(level & lv, int32_t recipe, const crafting_input & in) {
    const recipe_def & def = recipe_defs[recipe];
    switch (def.kind) {
        case RECIPE_KIND_SHAPED:
        case RECIPE_KIND_SHAPELESS:
            if (def.result_components) {
                level_unsupported(lv, "recipe %s: result data components", def.name);
                return {};
            }
            return { def.result, def.count };
        case RECIPE_KIND_TRANSMUTE:
            return transmute_recipe_assemble(recipe, in);
        case RECIPE_KIND_SPECIAL:
            break;
    }
    switch (def.cls) {
        case RCLASS_REPAIR_ITEM_RECIPE:
            return repair_item_recipe_assemble(in);
        case RCLASS_DECORATED_POT_RECIPE:
            return decorated_pot_recipe_assemble(lv, in);
        default:
            level_unsupported(lv, "%s.assemble", recipe_class_names[def.cls]);
            return {};
    }
}

// CraftingRecipe.defaultCraftingReminder; BannerDuplicateRecipe and BookCloningRecipe override it, but their assemble stops the run first
std::vector<item_stack> recipe_get_remaining_items(const crafting_input & in) {
    std::vector<item_stack> result;
    for (const item_stack & s : in.items) {
        int32_t remainder = item_defs[item_stack_get_item(s)].crafting_remainder;
        result.push_back(remainder < 0 ? item_stack() : item_stack{ remainder, 1 });
    }
    return result;
}
