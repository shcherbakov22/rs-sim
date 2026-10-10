// FaceAttachedHorizontalDirectionalBlock

#include "blocks/handlers.h"
#include "engine/level.h"

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
