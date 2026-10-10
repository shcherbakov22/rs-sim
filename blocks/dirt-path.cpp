// DirtPathBlock: turns back into dirt a tick after a solid block (other than a fence gate) lands on top

#include "blocks/handlers.h"
#include "engine/level.h"

bool dirt_path_can_survive(level & lv, block_pos pos) {
    state_id above = level_get_state(lv, pos_relative(pos, DIRECTION_UP));
    return !state_has(above, STATE_FLAG_SOLID) || block_instanceof(state_block(above), VCLASS_FENCE_GATE_BLOCK);
}

state_id dirt_path_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    if (dir == DIRECTION_UP && !block_can_survive(lv, st, pos)) {
        level_schedule_tick(lv, pos, state_block(st), 1);
    }
    return st;
}

// FarmBlock.turnToDirt
void dirt_path_tick(level & lv, block_pos pos) {
    level_set_block(lv, pos, block_default(BLOCK_DIRT), UPDATE_FLAG_ALL);
}
