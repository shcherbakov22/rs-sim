// BaseEntityBlock

#include "blocks/handlers.h"
#include "engine/level.h"

// BlockEntity.triggerEvent: chests and ender chests take event 1 for the lid, other block entities none
bool base_entity_block_trigger_event(level & lv, block_pos pos, int32_t param_a) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr) {
        return false;
    }
    bool lid = be->type == BLOCK_ENTITY_TYPE_CHEST || be->type == BLOCK_ENTITY_TYPE_TRAPPED_CHEST ||
               be->type == BLOCK_ENTITY_TYPE_ENDER_CHEST;
    return lid && param_a == 1;
}
