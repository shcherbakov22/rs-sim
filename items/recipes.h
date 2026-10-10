#pragma once

// Crafting recipes (net.minecraft.world.item.crafting), one section per sim/items/<class>.cpp. recipe-manager.cpp
// dispatches on the recipe class. Stacks in the sim carry only their item's default data components.

#include "core/item-stack.h"

#include <vector>

struct level;

// CraftingInput: the grid trimmed to its non-empty rows and columns
struct crafting_input {
    int32_t                 width;
    int32_t                 height;
    std::vector<item_stack> items;
    int32_t                 ingredient_count;  // non-empty stacks
};

crafting_input crafting_input_of(int32_t width, int32_t height, const item_stack * items);
item_stack     crafting_input_get_item(const crafting_input & in, int32_t x, int32_t y);

bool ingredient_test(int32_t ingredient, const item_stack & s);

// RecipeManager.getRecipeFor(RecipeType.CRAFTING): the first matching recipe, -1 for none
int32_t                 recipe_manager_get_recipe_for(level & lv, const crafting_input & in);
item_stack              recipe_assemble(level & lv, int32_t recipe, const crafting_input & in);
std::vector<item_stack> recipe_get_remaining_items(const crafting_input & in);

// ShapedRecipe (ShapedRecipePattern)
bool shaped_recipe_matches(int32_t recipe, const crafting_input & in);

// ShapelessRecipe
bool shapeless_recipe_matches(int32_t recipe, const crafting_input & in);

// TransmuteRecipe
bool       transmute_recipe_matches(level & lv, int32_t recipe, const crafting_input & in);
item_stack transmute_recipe_assemble(int32_t recipe, const crafting_input & in);

// MapExtendingRecipe
bool map_extending_recipe_matches(level & lv, int32_t recipe, const crafting_input & in);

// the special recipes (CustomRecipe)
bool       armor_dye_recipe_matches(const crafting_input & in);
bool       banner_duplicate_recipe_matches(const crafting_input & in);
bool       book_cloning_recipe_matches(const crafting_input & in);
bool       decorated_pot_recipe_matches(const crafting_input & in);
item_stack decorated_pot_recipe_assemble(level & lv, const crafting_input & in);
bool       firework_rocket_recipe_matches(const crafting_input & in);
bool       firework_star_fade_recipe_matches(const crafting_input & in);
bool       firework_star_recipe_matches(const crafting_input & in);
bool       map_cloning_recipe_matches(const crafting_input & in);
bool       repair_item_recipe_matches(const crafting_input & in);
item_stack repair_item_recipe_assemble(const crafting_input & in);
bool       shield_decoration_recipe_matches(const crafting_input & in);
bool       tipped_arrow_recipe_matches(const crafting_input & in);
