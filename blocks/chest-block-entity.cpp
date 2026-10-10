// ChestBlockEntity. Openers are players, and there are none: the opener count stays 0.

#include "blocks/handlers.h"
#include "engine/level.h"

// ChestBlockEntity.getOpenCount
int32_t chest_block_entity_get_open_count(level & lv, state_id st, block_pos pos) {
    if (state_has(st, STATE_FLAG_BLOCK_ENTITY)) {
        level_get_block_entity(lv, pos);
    }
    return 0;
}
