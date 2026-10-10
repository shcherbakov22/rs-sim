// BlockItem: places its block in the clicked block or next to it

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/handlers.h"

// BlockItem.canPlace; with no entities nothing obstructs a placement
static bool block_item_can_place(level & lv, const block_place_context & ctx, state_id st) {
    return !item_must_survive(lv, ctx) || block_can_survive(lv, st, block_place_context_get_clicked_pos(ctx));
}

state_id block_item_get_placement_state(level & lv, const block_place_context & ctx) {
    state_id st = block_get_state_for_placement(lv, item_defs[ctx.hit.stack.item].block, ctx);
    return st != STATE_NONE && block_item_can_place(lv, ctx, st) ? st : STATE_NONE;
}

bool block_item_place_block(level & lv, const block_place_context & ctx, state_id st) {
    return level_set_block(lv, block_place_context_get_clicked_pos(ctx), st, UPDATE_FLAG_ALL_IMMEDIATE);
}

// sounds, game events and the used up item are not in the trace
static interaction_result block_item_place(level & lv, const block_place_context & ctx) {
    if (!block_place_context_can_place(lv, ctx) || !item_update_placement_context(lv, ctx)) {
        return INTERACTION_RESULT_FAIL;
    }
    state_id st = item_get_placement_state(lv, ctx);
    if (st == STATE_NONE || !item_place_block(lv, ctx, st)) {
        return INTERACTION_RESULT_FAIL;
    }
    block_pos pos    = block_place_context_get_clicked_pos(ctx);
    state_id  placed = level_get_state(lv, pos);
    if (state_is(placed, state_block(st))) {
        if (item_defs[ctx.hit.stack.item].components & ITEM_COMPONENT_BLOCK_STATE) {
            level_unsupported(lv, "BLOCK_STATE component of %s", item_defs[ctx.hit.stack.item].name);
            return INTERACTION_RESULT_FAIL;
        }
        item_update_custom_block_entity_tag(lv, ctx);
        // applyComponentsFromItemStack changes nothing: a fresh stack has the defaults of a new block entity
        if (level_get_block_entity(lv, pos) != nullptr) {
            block_entity_set_changed(lv, pos, placed);
        }
        block_set_placed_by(lv, placed, pos);
    }
    return INTERACTION_RESULT_SUCCESS;
}

// a CONSUMABLE stack that fails to place falls back to Item.use, which only starts eating on the player
interaction_result block_item_use_on(level & lv, const use_on_context & hit) {
    return block_item_place(lv, block_place_context_init(lv, hit));
}
