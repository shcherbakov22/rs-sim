// ChestBlockEntity and TrappedChestBlockEntity. Openers are players, which only a use input adds, and use on
// a chest is unsupported: the opener count stays 0.

#include "blocks/handlers.h"
#include "engine/level.h"

// ContainerOpenersCounter.recheckOpeners with no openers: the count stays 0 and needs no recheck
void chest_block_entity_recheck_open(level & lv, block_entity * be) {
    // signalOpenCount sends the count even when it did not change; the trapped chest only updates on a change
    level_block_event(lv, be->pos, state_block(be->state), 1, 0);
}

// ChestBlockEntity.getOpenCount
int32_t chest_block_entity_get_open_count(level & lv, state_id st, block_pos pos) {
    if (state_has(st, STATE_FLAG_BLOCK_ENTITY)) {
        level_get_block_entity(lv, pos);
    }
    return 0;
}
