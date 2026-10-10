// ButtonBlock

#include "blocks/handlers.h"
#include "engine/level.h"

static void button_update_neighbours(level & lv, state_id st, block_pos pos) {
    direction front = direction_opposite(face_attached_horizontal_directional_get_connected_direction(st));
    level_update_neighbors_at(lv, pos, state_block(st));
    level_update_neighbors_at(lv, pos_relative(pos, front), state_block(st));
}

// ButtonBlock.press; the click sound and game event have no traced effect
interaction_result button_use_without_item(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_POWERED)) {
        return INTERACTION_RESULT_CONSUME;
    }
    int32_t block = state_block(st);
    level_set_block(lv, pos, state_set(st, PROP_POWERED, 1), UPDATE_FLAG_ALL);
    button_update_neighbours(lv, st, pos);
    level_schedule_tick(lv, pos, block, block_field_get(block, FIELD_BUTTON_BLOCK_TICKS_TO_STAY_PRESSED));
    return INTERACTION_RESULT_SUCCESS;
}

void button_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (!moved_by_piston && state_get(st, PROP_POWERED)) {
        button_update_neighbours(lv, st, pos);
    }
}

int32_t button_get_signal(state_id st) {
    return state_get(st, PROP_POWERED) ? 15 : 0;
}

int32_t button_get_direct_signal(state_id st, direction dir) {
    return state_get(st, PROP_POWERED) && face_attached_horizontal_directional_get_connected_direction(st) == dir ? 15 :
                                                                                                                    0;
}

// ButtonBlock.checkPressed: arrows are entities, which never exist in a traced run, so a pressed button releases
void button_tick(level & lv, state_id st, block_pos pos) {
    if (!state_get(st, PROP_POWERED)) {
        return;
    }
    level_set_block(lv, pos, state_set(st, PROP_POWERED, 0), UPDATE_FLAG_ALL);
    button_update_neighbours(lv, st, pos);
}
