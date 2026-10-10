// GrowingPlantBodyBlock: the plant behind its head; at the growing end it turns back into a head

#include "blocks/block-behaviour.h"
#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

// GrowingPlantBodyBlock.updateHeadAfterConvertedFromBody
static state_id growing_plant_body_update_head_after_converted_from_body(state_id body, state_id head) {
    if (block_instanceof(state_block(body), VCLASS_CAVE_VINES_PLANT_BLOCK)) {
        return cave_vines_plant_update_head_after_converted_from_body(body, head);
    }
    return head;
}

state_id growing_plant_body_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor) {
    int32_t   block  = state_block(st);
    int32_t   head   = block_defs[block].plant_head;
    direction growth = growing_plant_growth_direction(block);
    if (dir == direction_opposite(growth) && !block_can_survive(lv, st, pos)) {
        level_schedule_tick(lv, pos, block, 1);
    }
    if (dir == growth && !state_is(neighbor, block) && !state_is(neighbor, head)) {
        return growing_plant_body_update_head_after_converted_from_body(
            st, growing_plant_head_get_state_for_placement_random(lv, head));
    }
    if (block_field_get(block, FIELD_GROWING_PLANT_BLOCK_SCHEDULE_FLUID_TICKS)) {
        level_unsupported(lv, "fluids");
    }
    return st;
}

// the head's item never replaces the body
bool growing_plant_body_can_be_replaced(state_id st, const block_place_context & ctx) {
    int32_t head_item = block_defs[block_defs[state_block(st)].plant_head].as_item;
    return block_behaviour_can_be_replaced(st, ctx) && ctx.hit.stack.item != head_item;
}
