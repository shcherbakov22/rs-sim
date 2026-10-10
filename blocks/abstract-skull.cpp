// AbstractSkullBlock

#include "blocks/handlers.h"
#include "engine/level.h"

// flag 2 only: a powered skull changes state without updating its neighbors
void abstract_skull_neighbor_changed(level & lv, state_id st, block_pos pos) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (signal != (state_get(st, PROP_POWERED) != 0)) {
        level_set_block(lv, pos, state_set(st, PROP_POWERED, signal), UPDATE_FLAG_CLIENTS);
    }
}
