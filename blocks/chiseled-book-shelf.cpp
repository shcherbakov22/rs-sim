// ChiseledBookShelfBlock and ChiseledBookShelfBlockEntity. The slot properties are kept apart from the items: the use
// checks read the state, so a slot can show a book it does not hold.

#include "blocks/handlers.h"
#include "engine/level.h"

static block_entity * chiseled_book_shelf_block_entity(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    return be != nullptr && be->type == BLOCK_ENTITY_TYPE_CHISELED_BOOKSHELF ? be : nullptr;
}

// SelectableSlotContainer.getHitSlot: Inputs.use clicks the face center, which is row 1 (of 2) and column 1 (of 3)
static int32_t chiseled_book_shelf_get_hit_slot(state_id st, direction face) {
    return face == (direction) state_get(st, PROP_HORIZONTAL_FACING) ? 4 : -1;
}

static prop_id chiseled_book_shelf_slot_occupied(int32_t slot) {
    return (prop_id) (PROP_SLOT_0_OCCUPIED + slot);
}

// ChiseledBookShelfBlockEntity.updateState starts from the block entity's cached state
static void chiseled_book_shelf_block_entity_update_state(level & lv, block_entity * be, int32_t interacted_slot) {
    be->last_interacted_slot = interacted_slot;
    state_id updated         = be->state;
    for (int32_t slot = 0; slot < 6; slot++) {
        updated = state_set(updated, chiseled_book_shelf_slot_occupied(slot), !item_stack_is_empty(be->items[slot]));
    }
    level_set_block(lv, be->pos, updated, UPDATE_FLAG_ALL);
}

// ChiseledBookShelfBlockEntity.removeItem takes the whole slot and skips setChanged
item_stack chiseled_book_shelf_block_entity_remove_item(level & lv, block_entity * be, int32_t slot) {
    item_stack retrieved = be->items[slot];
    be->items[slot]      = item_stack();
    if (!item_stack_is_empty(retrieved)) {
        chiseled_book_shelf_block_entity_update_state(lv, be, slot);
    }
    return retrieved;
}

// ChiseledBookShelfBlockEntity.setItem drops items outside BOOKSHELF_BOOKS without a word
void chiseled_book_shelf_block_entity_set_item(level & lv, block_entity * be, int32_t slot, item_stack s) {
    if (item_defs[item_stack_get_item(s)].tags & ITEM_TAG_BOOKSHELF_BOOKS) {
        be->items[slot] = s;
        chiseled_book_shelf_block_entity_update_state(lv, be, slot);
    } else if (item_stack_is_empty(s)) {
        chiseled_book_shelf_block_entity_remove_item(lv, be, slot);
    }
}

// ListBackedContainer.canPlaceItem with getMaxStackSize 1
bool chiseled_book_shelf_block_entity_can_place_item(const block_entity * be, int32_t slot, const item_stack & s) {
    return (item_defs[item_stack_get_item(s)].tags & ITEM_TAG_BOOKSHELF_BOOKS) && item_stack_is_empty(be->items[slot]);
}

// ChiseledBookShelfBlockEntity.canTakeItem: the target needs an empty slot or a stack the item can join
bool chiseled_book_shelf_block_entity_can_take_item(const container & into, const item_stack & s) {
    for (int32_t slot = 0; slot < container_get_container_size(into); slot++) {
        const item_stack & to = container_get_item(into, slot);
        if (item_stack_is_empty(to) ||
            (item_stack_is_same_item_same_components(s, to) &&
             item_stack_get_count(to) + item_stack_get_count(s) <= container_get_max_stack_size(to))) {
            return true;
        }
    }
    return false;
}

int32_t chiseled_book_shelf_get_analog_output_signal(level & lv, block_pos pos) {
    block_entity * be = chiseled_book_shelf_block_entity(lv, pos);
    return be != nullptr ? be->last_interacted_slot + 1 : 0;
}

// ChiseledBookShelfBlock.addBook: the book is one taken from the held stack
interaction_result chiseled_book_shelf_use_item_on(level &            lv,
                                                   state_id           st,
                                                   block_pos          pos,
                                                   direction          face,
                                                   const item_stack & stack) {
    block_entity * be = chiseled_book_shelf_block_entity(lv, pos);
    if (be == nullptr) {
        return INTERACTION_RESULT_PASS;
    }
    if (!(item_defs[item_stack_get_item(stack)].tags & ITEM_TAG_BOOKSHELF_BOOKS)) {
        return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
    }
    int32_t slot = chiseled_book_shelf_get_hit_slot(st, face);
    if (slot < 0) {
        return INTERACTION_RESULT_PASS;
    }
    if (state_get(st, chiseled_book_shelf_slot_occupied(slot))) {
        return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
    }
    chiseled_book_shelf_block_entity_set_item(lv, be, slot, { stack.item, 1 });
    return INTERACTION_RESULT_SUCCESS;
}

// ChiseledBookShelfBlock.removeBook: the book goes to the player, who empties its inventory after the input
interaction_result chiseled_book_shelf_use_without_item(level & lv, state_id st, block_pos pos, direction face) {
    block_entity * be = chiseled_book_shelf_block_entity(lv, pos);
    if (be == nullptr) {
        return INTERACTION_RESULT_PASS;
    }
    int32_t slot = chiseled_book_shelf_get_hit_slot(st, face);
    if (slot < 0) {
        return INTERACTION_RESULT_PASS;
    }
    if (!state_get(st, chiseled_book_shelf_slot_occupied(slot))) {
        return INTERACTION_RESULT_CONSUME;
    }
    chiseled_book_shelf_block_entity_remove_item(lv, be, slot);
    return INTERACTION_RESULT_SUCCESS;
}
