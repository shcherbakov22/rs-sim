// EnderChestBlock and EnderChestBlockEntity. Openers are players, so the opener count stays 0.

#include "blocks/handlers.h"
#include "engine/level.h"

// EnderChestBlockEntity.recheckOpen: openerCountChanged sends the count even when it did not change
void ender_chest_tick(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be != nullptr && be->type == BLOCK_ENTITY_TYPE_ENDER_CHEST) {
        level_block_event(lv, pos, BLOCK_ENDER_CHEST, 1, 0);
    }
}
