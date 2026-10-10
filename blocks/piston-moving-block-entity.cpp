// PistonMovingBlockEntity

#include "blocks/handlers.h"
#include "engine/level.h"

#include <algorithm>

static float piston_moving_block_entity_get_extended_progress(const block_entity * be, float progress) {
    return be->extending ? progress - 1.0f : 1.0f - progress;
}

// the moved block at its current offset; a retracting piston also has its base, and a moving head the short arm
voxel_shape piston_moving_block_entity_get_collision_shape(level & lv, const block_entity * be) {
    voxel_shape head_shape;
    bool        moves_base = state_is(be->moved_state, BLOCK_PISTON) || state_is(be->moved_state, BLOCK_STICKY_PISTON);
    if (!be->extending && be->is_source_piston && moves_base) {
        head_shape = voxel_shape_collision(state_set(be->moved_state, PROP_EXTENDED, 1));
    }
    // NOCLIP is set only while entities move
    state_id st = be->moved_state;
    if (be->is_source_piston) {
        st = state_set(block_default(BLOCK_PISTON_HEAD), PROP_FACING, be->dir);
        st = state_set(st, PROP_SHORT, be->extending != (1.0f - be->progress < 0.25f));
    }
    if (state_collision_shapes[st] == SHAPE_NONE) {
        level_unsupported(lv, "dynamic shape of %s", state_to_string(st).c_str());
        return voxel_shape();
    }
    float             extended_progress = piston_moving_block_entity_get_extended_progress(be, be->progress);
    const block_pos & step              = direction_defs[be->dir].step;
    voxel_shape       moved             = voxel_shape_move(voxel_shape_collision(st), step.x * extended_progress,
                                                           step.y * extended_progress, step.z * extended_progress);
    return shapes_join(head_shape, moved, BOOLEAN_OP_OR);
}

// Ends the move at once: the moved block lands in its new place.
void piston_moving_block_entity_final_tick(level & lv, block_entity * be) {
    if (be->progress_o >= 1.0f) {
        return;
    }
    be->progress   = 1.0f;
    be->progress_o = be->progress;
    level_remove_block_entity(lv, be->pos);
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
