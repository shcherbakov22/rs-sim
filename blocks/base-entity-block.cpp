// BaseEntityBlock

#include "blocks/handlers.h"
#include "engine/level.h"

// BlockEntity.triggerEvent: event 1 moves chest, ender chest and shulker box lids, swings bells and wobbles decorated pots
bool base_entity_block_trigger_event(level & lv, state_id, block_pos pos, int32_t param_a, int32_t param_b) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || param_a != 1) {
        return false;
    }
    switch (be->type) {
        case BLOCK_ENTITY_TYPE_CHEST:
        case BLOCK_ENTITY_TYPE_TRAPPED_CHEST:
        case BLOCK_ENTITY_TYPE_ENDER_CHEST:
        case BLOCK_ENTITY_TYPE_SHULKER_BOX:
        case BLOCK_ENTITY_TYPE_BELL:
            return true;
        case BLOCK_ENTITY_TYPE_DECORATED_POT:
            // only the two WobbleStyle ordinals
            return param_b >= 0 && param_b < 2;
        default:
            return false;
    }
}
