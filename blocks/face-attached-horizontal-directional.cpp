// FaceAttachedHorizontalDirectionalBlock

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

direction face_attached_horizontal_directional_get_connected_direction(state_id st) {
    switch (state_get(st, PROP_ATTACH_FACE)) {
        case ATTACH_FACE_CEILING:
            return DIRECTION_DOWN;
        case ATTACH_FACE_FLOOR:
            return DIRECTION_UP;
        default:
            return (direction) state_get(st, PROP_HORIZONTAL_FACING);
    }
}

// FaceAttachedHorizontalDirectionalBlock.canAttach toward the opposite of the connected direction
bool face_attached_horizontal_directional_can_survive(level & lv, state_id st, block_pos pos) {
    direction dir = direction_opposite(face_attached_horizontal_directional_get_connected_direction(st));
    block_pos rel = pos_relative(pos, dir);
    return block_is_face_sturdy(lv, level_get_state(lv, rel), rel, direction_opposite(dir), SUPPORT_TYPE_FULL);
}

state_id face_attached_horizontal_directional_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    direction attached = direction_opposite(face_attached_horizontal_directional_get_connected_direction(st));
    return attached == dir && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

// the first looking direction with something to attach to
state_id face_attached_horizontal_directional_get_state_for_placement(level &                     lv,
                                                                      int32_t                     block,
                                                                      const block_place_context & ctx) {
    for (direction dir : block_place_context_get_nearest_looking_directions(ctx)) {
        state_id st;
        if (direction_get_axis(dir) == DIRECTION_AXIS_Y) {
            st = state_set(block_default(block), PROP_ATTACH_FACE,
                           dir == DIRECTION_UP ? ATTACH_FACE_CEILING : ATTACH_FACE_FLOOR);
            st = state_set(st, PROP_HORIZONTAL_FACING, player_direction);
        } else {
            st = state_set(block_default(block), PROP_ATTACH_FACE, ATTACH_FACE_WALL);
            st = state_set(st, PROP_HORIZONTAL_FACING, direction_opposite(dir));
        }
        if (block_can_survive(lv, st, block_place_context_get_clicked_pos(ctx))) {
            return st;
        }
    }
    return STATE_NONE;
}
