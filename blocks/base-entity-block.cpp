// BaseEntityBlock

#include "blocks/handlers.h"
#include "engine/level.h"

// BlockEntity.triggerEvent: chests take event 1 (EVENT_SET_OPEN_COUNT) for the lid, other block entities none
bool base_entity_block_trigger_event(level & lv, block_pos pos, int32_t param_a) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr) {
        return false;
    }
    return (be->type == BLOCK_ENTITY_TYPE_CHEST || be->type == BLOCK_ENTITY_TYPE_TRAPPED_CHEST) && param_a == 1;
}
