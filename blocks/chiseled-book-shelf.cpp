// ChiseledBookShelfBlock and ChiseledBookShelfBlockEntity. The slot properties are kept apart from the items, so a slot can show a book it does not hold.

#include "blocks/handlers.h"
#include "engine/level.h"

static block_entity * chiseled_book_shelf_block_entity(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    return be != nullptr && be->type == BLOCK_ENTITY_TYPE_CHISELED_BOOKSHELF ? be : nullptr;
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
