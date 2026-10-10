// RotatedPillarBlock: logs and pillars lie along the axis of the clicked face

#include "blocks/handlers.h"
#include "items/block-place-context.h"

state_id rotated_pillar_get_state_for_placement(int32_t block, const block_place_context & ctx) {
    return state_set(block_default(block), PROP_AXIS, direction_get_axis(ctx.hit.face));
}
