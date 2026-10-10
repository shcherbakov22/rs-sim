#include "items/item-behaviour.h"

#include "engine/level.h"
#include "items/handlers.h"

static int16_t item_hook_owner(int32_t item, item_hook h) {
    return iclass_hooks[item_defs[item].cls][h];
}

static void item_hook_unsupported(level & lv, int32_t item, item_hook h) {
    level_unsupported(lv, "%s.%s", iclass_names[item_hook_owner(item, h)], item_hook_names[h]);
}

interaction_result item_use_on(level & lv, const use_on_context & hit) {
    switch (item_hook_owner(hit.stack.item, ITEM_HOOK_USE_ON)) {
        case ICLASS_NONE:
            return INTERACTION_RESULT_PASS;
        case ICLASS_BLOCK_ITEM:
            return block_item_use_on(lv, hit);
        case ICLASS_HONEYCOMB_ITEM:
            return honeycomb_item_use_on(lv, hit);
        case ICLASS_AXE_ITEM:
            return axe_item_use_on(lv, hit);
        case ICLASS_FLINT_AND_STEEL_ITEM:
            return flint_and_steel_item_use_on(lv, hit);
        case ICLASS_FIRE_CHARGE_ITEM:
            return fire_charge_item_use_on(lv, hit);
        case ICLASS_ENDER_EYE_ITEM:
            return ender_eye_item_use_on(lv, hit);
        case ICLASS_SHEARS_ITEM:
            return shears_item_use_on(lv, hit);
        case ICLASS_SHOVEL_ITEM:
            return shovel_item_use_on(lv, hit);
        default:
            item_hook_unsupported(lv, hit.stack.item, ITEM_HOOK_USE_ON);
            return INTERACTION_RESULT_PASS;
    }
}

// false for a null context
bool item_update_placement_context(level & lv, const block_place_context & ctx) {
    switch (item_hook_owner(ctx.hit.stack.item, ITEM_HOOK_UPDATE_PLACEMENT_CONTEXT)) {
        case ICLASS_NONE:
            return true;
        default:
            item_hook_unsupported(lv, ctx.hit.stack.item, ITEM_HOOK_UPDATE_PLACEMENT_CONTEXT);
            return false;
    }
}

state_id item_get_placement_state(level & lv, const block_place_context & ctx) {
    switch (item_hook_owner(ctx.hit.stack.item, ITEM_HOOK_GET_PLACEMENT_STATE)) {
        case ICLASS_NONE:
            return block_item_get_placement_state(lv, ctx);
        case ICLASS_STANDING_AND_WALL_BLOCK_ITEM:
            return standing_and_wall_block_item_get_placement_state(lv, ctx);
        case ICLASS_GAME_MASTER_BLOCK_ITEM:
            // the virtual player cannot use game master blocks
            return STATE_NONE;
        default:
            item_hook_unsupported(lv, ctx.hit.stack.item, ITEM_HOOK_GET_PLACEMENT_STATE);
            return STATE_NONE;
    }
}

bool item_place_block(level & lv, const block_place_context & ctx, state_id st) {
    switch (item_hook_owner(ctx.hit.stack.item, ITEM_HOOK_PLACE_BLOCK)) {
        case ICLASS_NONE:
            return block_item_place_block(lv, ctx, st);
        case ICLASS_DOUBLE_HIGH_BLOCK_ITEM:
            return double_high_block_item_place_block(lv, ctx, st);
        default:
            item_hook_unsupported(lv, ctx.hit.stack.item, ITEM_HOOK_PLACE_BLOCK);
            return false;
    }
}

bool item_must_survive(level & lv, const block_place_context & ctx) {
    switch (item_hook_owner(ctx.hit.stack.item, ITEM_HOOK_MUST_SURVIVE)) {
        case ICLASS_NONE:
            return true;
        default:
            item_hook_unsupported(lv, ctx.hit.stack.item, ITEM_HOOK_MUST_SURVIVE);
            return true;
    }
}

// a fresh stack has no BLOCK_ENTITY_DATA, so BlockItem.updateCustomBlockEntityTag does nothing
void item_update_custom_block_entity_tag(level & lv, const block_place_context & ctx) {
    switch (item_hook_owner(ctx.hit.stack.item, ITEM_HOOK_UPDATE_CUSTOM_BLOCK_ENTITY_TAG)) {
        case ICLASS_NONE:
            return;
        default:
            item_hook_unsupported(lv, ctx.hit.stack.item, ITEM_HOOK_UPDATE_CUSTOM_BLOCK_ENTITY_TAG);
    }
}

// StandingAndWallBlockItem.canPlace
bool item_can_place(level & lv, const block_place_context & ctx, state_id st) {
    switch (item_hook_owner(ctx.hit.stack.item, ITEM_HOOK_CAN_PLACE)) {
        case ICLASS_NONE:
            return block_can_survive(lv, st, block_place_context_get_clicked_pos(ctx));
        default:
            item_hook_unsupported(lv, ctx.hit.stack.item, ITEM_HOOK_CAN_PLACE);
            return false;
    }
}
