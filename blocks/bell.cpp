// BellBlock. Without entities the bell block entity shows nothing: shaking and resonating only reach entities and the client.

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

static direction bell_facing(state_id st) {
    return (direction) state_get(st, PROP_HORIZONTAL_FACING);
}

static direction bell_get_connected_direction(state_id st) {
    switch (state_get(st, PROP_BELL_ATTACHMENT)) {
        case BELL_ATTACH_TYPE_FLOOR:
            return DIRECTION_UP;
        case BELL_ATTACH_TYPE_CEILING:
            return DIRECTION_DOWN;
        default:
            return direction_opposite(bell_facing(st));
    }
}

bool bell_can_survive(level & lv, state_id st, block_pos pos) {
    direction dir = direction_opposite(bell_get_connected_direction(st));
    if (dir == DIRECTION_UP) {
        return block_can_support_center(lv, pos_relative(pos, DIRECTION_UP), DIRECTION_DOWN);
    }
    // FaceAttachedHorizontalDirectionalBlock.canAttach
    block_pos rel = pos_relative(pos, dir);
    return block_is_face_sturdy(lv, level_get_state(lv, rel), rel, direction_opposite(dir), SUPPORT_TYPE_FULL);
}

// a double wall bell never breaks: it turns into a single wall bell facing away from the lost side
state_id bell_update_shape(level &   lv,
                           state_id  st,
                           block_pos pos,
                           direction dir,
                           block_pos neighbor_pos,
                           state_id  neighbor) {
    int32_t   attachment = state_get(st, PROP_BELL_ATTACHMENT);
    direction connected  = direction_opposite(bell_get_connected_direction(st));
    if (connected == dir && !block_can_survive(lv, st, pos) && attachment != BELL_ATTACH_TYPE_DOUBLE_WALL) {
        return STATE_AIR;
    }
    if (direction_get_axis(dir) == direction_get_axis(bell_facing(st))) {
        if (attachment == BELL_ATTACH_TYPE_DOUBLE_WALL &&
            !block_is_face_sturdy(lv, neighbor, neighbor_pos, dir, SUPPORT_TYPE_FULL)) {
            st = state_set(st, PROP_BELL_ATTACHMENT, BELL_ATTACH_TYPE_SINGLE_WALL);
            return state_set(st, PROP_HORIZONTAL_FACING, direction_opposite(dir));
        }
        if (attachment == BELL_ATTACH_TYPE_SINGLE_WALL && direction_opposite(connected) == dir &&
            block_is_face_sturdy(lv, neighbor, neighbor_pos, bell_facing(st), SUPPORT_TYPE_FULL)) {
            return state_set(st, PROP_BELL_ATTACHMENT, BELL_ATTACH_TYPE_DOUBLE_WALL);
        }
    }
    return st;
}

// BellBlock.attemptToRing and BellBlockEntity.onHit; the event carries the 3D data value of the direction
static void bell_attempt_to_ring(level & lv, block_pos pos, direction dir) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be != nullptr && be->type == BLOCK_ENTITY_TYPE_BELL) {
        level_block_event(lv, pos, BLOCK_BELL, 1, dir);
    }
}

void bell_neighbor_changed(level & lv, state_id st, block_pos pos) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (signal != (state_get(st, PROP_POWERED) != 0)) {
        if (signal) {
            bell_attempt_to_ring(lv, pos, bell_facing(level_get_state(lv, pos)));
        }
        level_set_block(lv, pos, state_set(st, PROP_POWERED, signal), UPDATE_FLAG_ALL);
    }
}

// BellBlock.onHit and isProperHit: Inputs.use clicks the face center, below the 0.8124 rim
interaction_result bell_use_without_item(level & lv, state_id st, block_pos pos, direction face) {
    if (direction_get_axis(face) == DIRECTION_AXIS_Y) {
        return INTERACTION_RESULT_PASS;
    }
    bool same_axis = direction_get_axis(bell_facing(st)) == direction_get_axis(face);
    bool proper    = false;
    switch (state_get(st, PROP_BELL_ATTACHMENT)) {
        case BELL_ATTACH_TYPE_FLOOR:
            proper = same_axis;
            break;
        case BELL_ATTACH_TYPE_CEILING:
            proper = true;
            break;
        default:
            proper = !same_axis;
    }
    if (!proper) {
        return INTERACTION_RESULT_PASS;
    }
    bell_attempt_to_ring(lv, pos, face);
    return INTERACTION_RESULT_SUCCESS;
}

// on a side face a bell hangs between two sturdy faces across it, else on the one behind, else stands or hangs
state_id bell_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    direction face = ctx.hit.face;
    block_pos pos  = block_place_context_get_clicked_pos(ctx);
    if (direction_get_axis(face) == DIRECTION_AXIS_Y) {
        state_id st = state_set(block_default(block), PROP_BELL_ATTACHMENT,
                                face == DIRECTION_DOWN ? BELL_ATTACH_TYPE_CEILING : BELL_ATTACH_TYPE_FLOOR);
        st          = state_set(st, PROP_HORIZONTAL_FACING, player_direction);
        return block_can_survive(lv, st, pos) ? st : STATE_NONE;
    }
    direction left      = direction_get_axis(face) == DIRECTION_AXIS_X ? DIRECTION_WEST : DIRECTION_NORTH;
    direction right     = direction_opposite(left);
    block_pos left_pos  = pos_relative(pos, left);
    block_pos right_pos = pos_relative(pos, right);
    bool      double_attached =
        block_is_face_sturdy(lv, level_get_state(lv, left_pos), left_pos, right, SUPPORT_TYPE_FULL) &&
        block_is_face_sturdy(lv, level_get_state(lv, right_pos), right_pos, left, SUPPORT_TYPE_FULL);
    state_id st = state_set(block_default(block), PROP_HORIZONTAL_FACING, direction_opposite(face));
    st          = state_set(st, PROP_BELL_ATTACHMENT,
                            double_attached ? BELL_ATTACH_TYPE_DOUBLE_WALL : BELL_ATTACH_TYPE_SINGLE_WALL);
    if (block_can_survive(lv, st, pos)) {
        return st;
    }
    block_pos below = pos_relative(pos, DIRECTION_DOWN);
    bool      can_attach_below =
        block_is_face_sturdy(lv, level_get_state(lv, below), below, DIRECTION_UP, SUPPORT_TYPE_FULL);
    st = state_set(st, PROP_BELL_ATTACHMENT, can_attach_below ? BELL_ATTACH_TYPE_FLOOR : BELL_ATTACH_TYPE_CEILING);
    return block_can_survive(lv, st, pos) ? st : STATE_NONE;
}
