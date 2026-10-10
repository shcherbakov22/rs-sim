// SoulFireBlock: fire that lives only on soul fire base blocks

#include "blocks/handlers.h"
#include "engine/level.h"

bool soul_fire_can_survive_on_block(state_id st) {
    return state_has(st, STATE_FLAG_TAG_SOUL_FIRE_BASE_BLOCKS);
}

bool soul_fire_can_survive(level & lv, block_pos pos) {
    return soul_fire_can_survive_on_block(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)));
}

state_id soul_fire_update_shape(level & lv, state_id st, block_pos pos) {
    return soul_fire_can_survive(lv, pos) ? block_default(state_block(st)) : STATE_AIR;
}
