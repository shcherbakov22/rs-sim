// GrowingPlantBlock: a vine or kelp column; the head (with an age) is at the growing end and the body fills behind it

#include "blocks/block-behaviour.h"
#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

direction growing_plant_growth_direction(int32_t block) {
    return (direction) block_field_get(block, FIELD_GROWING_PLANT_BLOCK_GROWTH_DIRECTION);
}

// GrowingPlantBlock.canAttachTo
static bool growing_plant_can_attach_to(int32_t block, state_id attached) {
    if (block_instanceof(block, VCLASS_KELP_BLOCK) || block_instanceof(block, VCLASS_KELP_PLANT_BLOCK)) {
        return kelp_can_attach_to(attached);
    }
    return true;
}

// the block it grows from: another part of the same plant, or a sturdy face
bool growing_plant_can_survive(level & lv, state_id st, block_pos pos) {
    int32_t   block        = state_block(st);
    direction growth       = growing_plant_growth_direction(block);
    block_pos attached_pos = pos_relative(pos, direction_opposite(growth));
    state_id  attached     = level_get_state(lv, attached_pos);
    if (!growing_plant_can_attach_to(block, attached)) {
        return false;
    }
    return state_is(attached, block_defs[block].plant_head) || state_is(attached, block_defs[block].plant_body) ||
           block_is_face_sturdy(lv, attached, attached_pos, growth, SUPPORT_TYPE_FULL);
}

// GrowingPlantBlock.getStateForPlacement(RandomSource); the head overrides it
static state_id growing_plant_get_state_for_placement_random(level & lv, int32_t block) {
    if (block_instanceof(block, VCLASS_GROWING_PLANT_HEAD_BLOCK)) {
        return growing_plant_head_get_state_for_placement_random(lv, block);
    }
    return block_default(block);
}

// placed before more of the plant, it becomes body
state_id growing_plant_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    block_pos at   = pos_relative(block_place_context_get_clicked_pos(ctx), growing_plant_growth_direction(block));
    state_id  next = level_get_state(lv, at);
    if (state_is(next, block_defs[block].plant_head) || state_is(next, block_defs[block].plant_body)) {
        return block_default(block_defs[block].plant_body);
    }
    return growing_plant_get_state_for_placement_random(lv, block);
}

void growing_plant_tick(level & lv, state_id st, block_pos pos) {
    if (!block_can_survive(lv, st, pos)) {
        level_destroy_block(lv, pos, true);
    }
}
