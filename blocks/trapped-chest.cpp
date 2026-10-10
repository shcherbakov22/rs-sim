// TrappedChestBlock

#include "blocks/handlers.h"
#include "engine/level.h"

#include <algorithm>

int32_t trapped_chest_get_signal(level & lv, state_id st, block_pos pos) {
    return std::clamp(chest_block_entity_get_open_count(lv, st, pos), 0, 15);
}

int32_t trapped_chest_get_direct_signal(level & lv, state_id st, block_pos pos, direction dir) {
    return dir == DIRECTION_UP ? trapped_chest_get_signal(lv, st, pos) : 0;
}
