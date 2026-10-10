// EnderChestBlock and EnderChestBlockEntity. Openers are players, so the opener count stays 0.

#include "blocks/handlers.h"
#include "engine/level.h"

state_id ender_chest_update_shape(level & lv, state_id st) {
    if (state_get(st, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
    }
    return st;
}

// EnderChestBlockEntity.recheckOpen: openerCountChanged sends the count even when it did not change
void ender_chest_tick(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be != nullptr && be->type == BLOCK_ENTITY_TYPE_ENDER_CHEST) {
        level_block_event(lv, pos, BLOCK_ENDER_CHEST, 1, 0);
    }
}

// a conductor above keeps the chest shut without opening a menu
interaction_result ender_chest_use_without_item(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || be->type != BLOCK_ENTITY_TYPE_ENDER_CHEST ||
        state_has(level_get_state(lv, pos_relative(pos, DIRECTION_UP)), STATE_FLAG_CONDUCTOR)) {
        return INTERACTION_RESULT_SUCCESS;
    }
    level_unsupported(lv, "container menus");
    return INTERACTION_RESULT_SUCCESS;
}
