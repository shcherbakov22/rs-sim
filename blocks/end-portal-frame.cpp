// EndPortalFrameBlock

#include "blocks/handlers.h"

state_id end_portal_frame_get_state_for_placement(int32_t block) {
    state_id st = state_set(block_default(block), PROP_HORIZONTAL_FACING, direction_opposite(player_direction));
    return state_set(st, PROP_EYE, 0);
}

int32_t end_portal_frame_get_analog_output_signal(state_id st) {
    return state_get(st, PROP_EYE) ? 15 : 0;
}
