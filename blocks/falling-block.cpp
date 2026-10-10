// FallingBlock: sand, gravel, concrete powder and the like fall when the block below is free. The fall spawns an entity, which ends the run.

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"

// getDelayAfterPlace
static const int32_t FALLING_BLOCK_DELAY_AFTER_PLACE = 2;

void falling_block_on_place(level & lv, state_id st, block_pos pos) {
    level_schedule_tick(lv, pos, state_block(st), FALLING_BLOCK_DELAY_AFTER_PLACE);
}

state_id falling_block_update_shape(level & lv, state_id st, block_pos pos) {
    level_schedule_tick(lv, pos, state_block(st), FALLING_BLOCK_DELAY_AFTER_PLACE);
    return st;
}

// FallingBlockEntity.fall clears the block before the spawn; falling blocks hold no fluid, so the block left is air
void falling_block_tick(level & lv, block_pos pos) {
    if (!state_has(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)), STATE_FLAG_FALLING_FREE) ||
        pos.y < LEVEL_Y_MIN) {
        return;
    }
    level_set_block(lv, pos, STATE_AIR, UPDATE_FLAG_ALL);
    trace_boundary(lv.tr, "spawn falling_block %d %d %d", pos.x, pos.y, pos.z);
}
