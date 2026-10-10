// DoorBlock: two halves that copy each other through updateShape; a use (not for iron) or a signal opens it

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

// the sound pitch draws a float
static void door_play_sound(level & lv) {
    rng_next_float(lv.random);
}

bool door_can_survive(level & lv, state_id st, block_pos pos) {
    block_pos below       = pos_relative(pos, DIRECTION_DOWN);
    state_id  below_state = level_get_state(lv, below);
    if (state_get(st, PROP_DOUBLE_BLOCK_HALF) == DOUBLE_BLOCK_HALF_LOWER) {
        return block_is_face_sturdy(lv, below_state, below, DIRECTION_UP, SUPPORT_TYPE_FULL);
    }
    return state_block(below_state) == state_block(st);
}

// toward the other half the door takes the other half's state; a half without its partner breaks
state_id door_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor) {
    int32_t half = state_get(st, PROP_DOUBLE_BLOCK_HALF);
    if (direction_get_axis(dir) != DIRECTION_AXIS_Y || (half == DOUBLE_BLOCK_HALF_LOWER) != (dir == DIRECTION_UP)) {
        if (half == DOUBLE_BLOCK_HALF_LOWER && dir == DIRECTION_DOWN && !door_can_survive(lv, st, pos)) {
            return STATE_AIR;
        }
        return st;
    }
    if (block_instanceof(state_block(neighbor), VCLASS_DOOR_BLOCK) &&
        state_get(neighbor, PROP_DOUBLE_BLOCK_HALF) != half) {
        return state_set(neighbor, PROP_DOUBLE_BLOCK_HALF, half);
    }
    return STATE_AIR;
}

interaction_result door_use_without_item(level & lv, state_id st, block_pos pos) {
    if (!block_field_get(state_block(st), FIELD_DOOR_BLOCK_TYPE_CAN_OPEN_BY_HAND)) {
        return INTERACTION_RESULT_PASS;
    }
    st = state_set(st, PROP_OPEN, !state_get(st, PROP_OPEN));
    level_set_block(lv, pos, st, UPDATE_FLAG_CLIENTS | UPDATE_FLAG_IMMEDIATE);
    door_play_sound(lv);
    return INTERACTION_RESULT_SUCCESS;
}

// either half reads the signal at both; an update from a block of the same door type is ignored
void door_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block) {
    direction other  = state_get(st, PROP_DOUBLE_BLOCK_HALF) == DOUBLE_BLOCK_HALF_LOWER ? DIRECTION_UP : DIRECTION_DOWN;
    bool      signal = level_has_neighbor_signal(lv, pos) || level_has_neighbor_signal(lv, pos_relative(pos, other));
    if (block == state_block(st) || signal == (bool) state_get(st, PROP_POWERED)) {
        return;
    }
    if (signal != (bool) state_get(st, PROP_OPEN)) {
        door_play_sound(lv);
    }
    level_set_block(lv, pos, state_set(state_set(st, PROP_POWERED, signal), PROP_OPEN, signal), UPDATE_FLAG_CLIENTS);
}

// DoorBlock.getHinge: full blocks beside the door push the hinge away from them, a door beside it pairs up, else the
// click decides
static door_hinge_side door_get_hinge(level & lv, const block_place_context & ctx) {
    block_pos pos                 = block_place_context_get_clicked_pos(ctx);
    direction place_direction     = player_direction;
    block_pos above               = pos_relative(pos, DIRECTION_UP);
    direction left_direction      = direction_counter_clockwise(place_direction);
    block_pos left_pos            = pos_relative(pos, left_direction);
    state_id  left                = level_get_state(lv, left_pos);
    block_pos left_above_pos      = pos_relative(above, left_direction);
    state_id  left_above          = level_get_state(lv, left_above_pos);
    direction right_direction     = direction_clockwise(place_direction);
    block_pos right_pos           = pos_relative(pos, right_direction);
    state_id  right               = level_get_state(lv, right_pos);
    block_pos right_above_pos     = pos_relative(above, right_direction);
    state_id  right_above         = level_get_state(lv, right_above_pos);
    int32_t   solid_block_balance = (block_is_collision_shape_full_block(lv, left, left_pos) ? -1 : 0) +
                                    (block_is_collision_shape_full_block(lv, left_above, left_above_pos) ? -1 : 0) +
                                    (block_is_collision_shape_full_block(lv, right, right_pos) ? 1 : 0) +
                                    (block_is_collision_shape_full_block(lv, right_above, right_above_pos) ? 1 : 0);
    bool      door_left           = block_instanceof(state_block(left), VCLASS_DOOR_BLOCK) &&
                                    state_get(left, PROP_DOUBLE_BLOCK_HALF) == DOUBLE_BLOCK_HALF_LOWER;
    bool      door_right          = block_instanceof(state_block(right), VCLASS_DOOR_BLOCK) &&
                                    state_get(right, PROP_DOUBLE_BLOCK_HALF) == DOUBLE_BLOCK_HALF_LOWER;
    if ((door_left && !door_right) || solid_block_balance > 0) {
        return DOOR_HINGE_SIDE_RIGHT;
    }
    if ((door_right && !door_left) || solid_block_balance < 0) {
        return DOOR_HINGE_SIDE_LEFT;
    }
    const block_pos & step       = direction_defs[place_direction].step;
    double            click_x    = use_on_context_get_click_location(ctx.hit, DIRECTION_AXIS_X) - pos.x;
    double            click_z    = use_on_context_get_click_location(ctx.hit, DIRECTION_AXIS_Z) - pos.z;
    bool              left_hinge = (step.x >= 0 || !(click_z < 0.5)) && (step.x <= 0 || !(click_z > 0.5)) &&
                                   (step.z >= 0 || !(click_x > 0.5)) && (step.z <= 0 || !(click_x < 0.5));
    return left_hinge ? DOOR_HINGE_SIDE_LEFT : DOOR_HINGE_SIDE_RIGHT;
}

state_id door_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    block_pos pos   = block_place_context_get_clicked_pos(ctx);
    block_pos above = pos_relative(pos, DIRECTION_UP);
    if (pos.y >= LEVEL_Y_MAX || !block_can_be_replaced(lv, level_get_state(lv, above), ctx)) {
        return STATE_NONE;
    }
    bool     powered = level_has_neighbor_signal(lv, pos) || level_has_neighbor_signal(lv, above);
    state_id st      = state_set(block_default(block), PROP_HORIZONTAL_FACING, player_direction);
    st               = state_set(st, PROP_DOOR_HINGE, door_get_hinge(lv, ctx));
    st               = state_set(state_set(st, PROP_POWERED, powered), PROP_OPEN, powered);
    return state_set(st, PROP_DOUBLE_BLOCK_HALF, DOUBLE_BLOCK_HALF_LOWER);
}

void door_set_placed_by(level & lv, state_id st, block_pos pos) {
    level_set_block(lv, pos_relative(pos, DIRECTION_UP), state_set(st, PROP_DOUBLE_BLOCK_HALF, DOUBLE_BLOCK_HALF_UPPER),
                    UPDATE_FLAG_ALL);
}
