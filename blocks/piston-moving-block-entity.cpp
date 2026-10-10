// PistonMovingBlockEntity

#include "blocks/handlers.h"
#include "engine/level.h"

#include <algorithm>

// Ends the move at once: the moved block lands in its new place.
void piston_moving_block_entity_final_tick(level & lv, block_entity * be) {
    if (be->progress_o >= 1.0f) {
        return;
    }
    be->progress   = 1.0f;
    be->progress_o = be->progress;
    level_remove_block_entity(lv, be->pos);
    be->removed = true;
    if (state_is(level_get_state(lv, be->pos), BLOCK_MOVING_PISTON)) {
        state_id st =
            be->is_source_piston ? STATE_AIR : block_update_from_neighbour_shapes(lv, be->moved_state, be->pos);
        level_set_block(lv, be->pos, st, UPDATE_FLAG_ALL);
        level_neighbor_changed(lv, be->pos, state_block(st));
    }
}

// Entities are out of scope, so a tick only advances the progress (0, 0.5, 1) and then lands the block.
void piston_moving_block_entity_tick(level & lv, block_entity * be) {
    be->last_ticked = lv.game_time;
    be->progress_o  = be->progress;
    if (be->progress_o < 1.0f) {
        be->progress = std::min(be->progress + 0.5f, 1.0f);
        return;
    }
    block_pos pos = be->pos;
    level_remove_block_entity(lv, pos);
    be->removed = true;
    if (!state_is(level_get_state(lv, pos), BLOCK_MOVING_PISTON)) {
        return;
    }
    state_id st = block_update_from_neighbour_shapes(lv, be->moved_state, pos);
    if (state_has(st, STATE_FLAG_AIR)) {
        // the block lands silently, then breaks as if a shape update removed it
        level_set_block(lv, pos, be->moved_state,
                        UPDATE_FLAG_INVISIBLE | UPDATE_FLAG_KNOWN_SHAPE | UPDATE_FLAG_MOVE_BY_PISTON |
                            UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
        block_update_or_destroy(lv, be->moved_state, st, pos, UPDATE_FLAG_ALL, UPDATE_LIMIT);
        return;
    }
    if (state_has_prop(st, PROP_WATERLOGGED) && state_get(st, PROP_WATERLOGGED)) {
        st = state_set(st, PROP_WATERLOGGED, 0);
    }
    level_set_block(lv, pos, st, UPDATE_FLAG_ALL | UPDATE_FLAG_MOVE_BY_PISTON);
    level_neighbor_changed(lv, pos, state_block(st));
}
