// LecternBlock and LecternBlockEntity. Pages live in item components, which the sim does not load: every book has 0 pages.

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"

static void lectern_update_below(level & lv, state_id st, block_pos pos) {
    level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_DOWN), state_block(st));
}

static void lectern_change_powered(level & lv, state_id st, block_pos pos, bool powered) {
    level_set_block(lv, pos, state_set(st, PROP_POWERED, powered), UPDATE_FLAG_ALL);
    lectern_update_below(lv, st, pos);
}

void lectern_tick(level & lv, state_id st, block_pos pos) {
    lectern_change_powered(lv, st, pos, false);
}

void lectern_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_POWERED)) {
        lectern_update_below(lv, st, pos);
    }
}

int32_t lectern_get_signal(state_id st) {
    return state_get(st, PROP_POWERED) ? 15 : 0;
}

int32_t lectern_get_direct_signal(state_id st, direction dir) {
    return dir == DIRECTION_UP && state_get(st, PROP_POWERED) ? 15 : 0;
}

// LecternBlockEntity.getRedstoneSignal: without pages the page progress is 1; HAS_BOOK can be set without a book
int32_t lectern_get_analog_output_signal(level & lv, state_id st, block_pos pos) {
    block_entity * be = state_get(st, PROP_HAS_BOOK) ? level_get_block_entity(lv, pos) : nullptr;
    if (be == nullptr || be->type != BLOCK_ENTITY_TYPE_LECTERN) {
        return 0;
    }
    return 14 + (item_stack_is_empty(be->items[0]) ? 0 : 1);
}

// LecternBlock.tryPlaceBook, placeBook and resetBookState
static void lectern_place_book(level & lv, state_id st, block_pos pos, const item_stack & book) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || be->type != BLOCK_ENTITY_TYPE_LECTERN) {
        return;
    }
    be->items[0] = { book.item, 1 };
    block_entity_set_changed(lv, pos, be->state);
    level_set_block(lv, pos, state_set(state_set(st, PROP_POWERED, 0), PROP_HAS_BOOK, 1), UPDATE_FLAG_ALL);
    lectern_update_below(lv, st, pos);
}

interaction_result lectern_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack) {
    if (state_get(st, PROP_HAS_BOOK)) {
        return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
    }
    if (item_defs[item_stack_get_item(stack)].tags & ITEM_TAG_LECTERN_BOOKS) {
        lectern_place_book(lv, st, pos, stack);
        return INTERACTION_RESULT_SUCCESS;
    }
    return item_stack_is_empty(stack) ? INTERACTION_RESULT_PASS : INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
}

interaction_result lectern_use_without_item(level & lv, state_id st) {
    if (state_get(st, PROP_HAS_BOOK)) {
        level_unsupported(lv, "container menus");
        return INTERACTION_RESULT_SUCCESS;
    }
    return INTERACTION_RESULT_CONSUME;
}

// LecternBlockEntity.preRemoveSideEffects: the book pops out on top, even an empty one
void lectern_block_entity_pre_remove_side_effects(level & lv, block_pos pos, state_id st) {
    if (state_get(st, PROP_HAS_BOOK)) {
        rng_next_double(lv.random);
        rng_next_double(lv.random);
        block_pos at = pos_relative(pos, DIRECTION_UP);
        trace_boundary(lv.tr, "spawn item %d %d %d", at.x, at.y, at.z);
    }
}
