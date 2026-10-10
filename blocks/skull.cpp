// SkullBlock: a standing skull, rotated to the player's yaw

#include "blocks/handlers.h"

state_id skull_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    return state_set(abstract_skull_get_state_for_placement(lv, block, ctx), PROP_ROTATION_16, player_rotation_segment);
}
