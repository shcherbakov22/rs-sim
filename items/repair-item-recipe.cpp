// RepairItemRecipe: two single damageable items of the same kind

#include "items/recipes.h"

// RepairItemRecipe.canCombine
static bool repair_item_recipe_can_combine(const item_stack & first, const item_stack & second) {
    return second.item == first.item && first.count == 1 && second.count == 1 && item_stack_is_damageable_item(first) &&
           item_stack_is_damageable_item(second);
}

// RepairItemRecipe.getItemsToCombine: false for null
static bool repair_item_recipe_get_items_to_combine(const crafting_input & in,
                                                    const item_stack *&    first,
                                                    const item_stack *&    second) {
    if (in.ingredient_count != 2) {
        return false;
    }
    first = nullptr;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) {
            continue;
        }
        if (first != nullptr) {
            second = &s;
            return repair_item_recipe_can_combine(*first, s);
        }
        first = &s;
    }
    return false;
}

bool repair_item_recipe_matches(const crafting_input & in) {
    const item_stack * first;
    const item_stack * second;
    return repair_item_recipe_get_items_to_combine(in, first, second);
}

// both items are of one kind, so they share MAX_DAMAGE; stacks are unenchanted, so no curses carry over
item_stack repair_item_recipe_assemble(const crafting_input & in) {
    const item_stack * first;
    const item_stack * second;
    if (!repair_item_recipe_get_items_to_combine(in, first, second)) {
        return item_stack();
    }
    int32_t durability = item_defs[first->item].max_damage;
    int32_t remaining  = durability - first->damage + durability - second->damage + durability * 5 / 100;
    return item_stack{ first->item, 1, std::max(durability - remaining, 0) };
}
