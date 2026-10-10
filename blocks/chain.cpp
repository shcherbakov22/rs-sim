// ChainBlock: a rotated pillar that can be waterlogged

#include "blocks/handlers.h"
#include "items/block-place-context.h"

state_id chain_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    bool water = block_place_context_is_water_source(lv, ctx);
    return state_set(rotated_pillar_get_state_for_placement(block, ctx), PROP_WATERLOGGED, water);
}
