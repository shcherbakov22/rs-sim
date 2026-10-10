// BookCloningRecipe: a written book copies into book and quills

#include "items/recipes.h"

bool book_cloning_recipe_matches(const crafting_input & in) {
    if (in.ingredient_count < 2) {
        return false;
    }
    bool has_empty_books = false;
    bool has_source_book = false;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) {
            continue;
        }
        if (item_defs[s.item].components & ITEM_COMPONENT_WRITTEN_BOOK_CONTENT) {
            if (has_source_book) {
                return false;
            }
            has_source_book = true;
        } else {
            if (!(item_defs[s.item].tags & ITEM_TAG_BOOK_CLONING_TARGET)) {
                return false;
            }
            has_empty_books = true;
        }
    }
    return has_source_book && has_empty_books;
}
