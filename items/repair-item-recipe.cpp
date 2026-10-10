// RepairItemRecipe: two single damageable items of the same kind

#include "items/recipes.h"

static bool repair_item_recipe_has_damage(const item_stack & s) {
    uint32_t components = item_defs[s.item].components;
    return (components & ITEM_COMPONENT_MAX_DAMAGE) && (components & ITEM_COMPONENT_DAMAGE);
}

// RepairItemRecipe.canCombine
static bool repair_item_recipe_can_combine(const item_stack & first, const item_stack & second) {
    return second.item == first.item && first.count == 1 && second.count == 1 && repair_item_recipe_has_damage(first) &&
           repair_item_recipe_has_damage(second);
}

// RepairItemRecipe.getItemsToCombine: the first of the pair, nullptr for null
static const item_stack * repair_item_recipe_get_items_to_combine(const crafting_input & in) {
    if (in.ingredient_count != 2) {
        return nullptr;
    }
    const item_stack * first = nullptr;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) {
            continue;
        }
        if (first != nullptr) {
            return repair_item_recipe_can_combine(*first, s) ? first : nullptr;
        }
        first = &s;
    }
    return nullptr;
}

bool repair_item_recipe_matches(const crafting_input & in) {
    return repair_item_recipe_get_items_to_combine(in) != nullptr;
}

// both items are undamaged and unenchanted, so the summed durability gives damage 0 and no curses carry over: the result has only default components
item_stack repair_item_recipe_assemble(const crafting_input & in) {
    const item_stack * first = repair_item_recipe_get_items_to_combine(in);
    return first == nullptr ? item_stack() : item_stack{ first->item, 1 };
}
