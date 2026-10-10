// LeverBlock

#include "blocks/handlers.h"
#include "engine/level.h"

static void lever_update_neighbours(level & lv, state_id st, block_pos pos) {
    direction front = direction_opposite(face_attached_horizontal_directional_get_connected_direction(st));
    level_update_neighbors_at(lv, pos, state_block(st));
    level_update_neighbors_at(lv, pos_relative(pos, front), state_block(st));
}

// LeverBlock.pull; the sound and game event have no traced effect
void lever_pull(level & lv, state_id st, block_pos pos) {
    st = state_cycle(st, PROP_POWERED);
    level_set_block(lv, pos, st, UPDATE_FLAG_ALL);
    lever_update_neighbours(lv, st, pos);
}

void lever_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (!moved_by_piston && state_get(st, PROP_POWERED)) {
        lever_update_neighbours(lv, st, pos);
    }
}

int32_t lever_get_signal(state_id st) {
    return state_get(st, PROP_POWERED) ? 15 : 0;
}

int32_t lever_get_direct_signal(state_id st, direction dir) {
    return state_get(st, PROP_POWERED) && face_attached_horizontal_directional_get_connected_direction(st) == dir ? 15 :
                                                                                                                    0;
}
