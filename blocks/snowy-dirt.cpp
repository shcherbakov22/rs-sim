// SnowyDirtBlock: grass, podzol and mycelium show snow while a block tagged snow is on top

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

state_id snowy_dirt_update_shape(state_id st, direction dir, state_id neighbor) {
    return dir == DIRECTION_UP ? state_set(st, PROP_SNOWY, state_has(neighbor, STATE_FLAG_TAG_SNOW)) : st;
}

state_id snowy_dirt_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    state_id above = level_get_state(lv, pos_relative(block_place_context_get_clicked_pos(ctx), DIRECTION_UP));
    return state_set(block_default(block), PROP_SNOWY, state_has(above, STATE_FLAG_TAG_SNOW));
}
