// AbstractSkullBlock

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

// flag 2 only: a powered skull changes state without updating its neighbors
void abstract_skull_neighbor_changed(level & lv, state_id st, block_pos pos) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (signal != (state_get(st, PROP_POWERED) != 0)) {
        level_set_block(lv, pos, state_set(st, PROP_POWERED, signal), UPDATE_FLAG_CLIENTS);
    }
}

state_id abstract_skull_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    return state_set(block_default(block), PROP_POWERED,
                     level_has_neighbor_signal(lv, block_place_context_get_clicked_pos(ctx)));
}
