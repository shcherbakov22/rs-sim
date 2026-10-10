#pragma once

// Crafting recipes (net.minecraft.world.item.crafting), in recipes.cpp

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

// RecipeManager.getRecipeFor(RecipeType.CRAFTING): the first matching recipe, -1 for none
int32_t                 recipe_manager_get_recipe_for(level & lv, const crafting_input & in);
item_stack              recipe_assemble(level & lv, int32_t recipe, const crafting_input & in);
std::vector<item_stack> recipe_get_remaining_items(const crafting_input & in);
