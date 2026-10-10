// GrowingPlantHeadBlock: the growing end of a plant; it turns into body once more plant is past it

#include "blocks/handlers.h"
#include "engine/level.h"

// a fresh head draws its age
state_id growing_plant_head_get_state_for_placement_random(level & lv, int32_t block) {
    return state_set(block_default(block), PROP_AGE_25, rng_next_int(lv.random, 25));
}

// GrowingPlantHeadBlock.updateBodyAfterConvertedFromHead
static state_id growing_plant_head_update_body_after_converted_from_head(state_id head, state_id body) {
    if (block_instanceof(state_block(head), VCLASS_CAVE_VINES_BLOCK)) {
        return cave_vines_update_body_after_converted_from_head(head, body);
    }
    return body;
}

state_id growing_plant_head_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor) {
    int32_t   block  = state_block(st);
    int32_t   body   = block_defs[block].plant_body;
    direction growth = growing_plant_growth_direction(block);
    if (dir == direction_opposite(growth)) {
        if (!block_can_survive(lv, st, pos)) {
            level_schedule_tick(lv, pos, block, 1);
        } else {
            state_id next = level_get_state(lv, pos_relative(pos, growth));
            if (state_is(next, block) || state_is(next, body)) {
                return growing_plant_head_update_body_after_converted_from_head(st, block_default(body));
            }
        }
    }
    if (dir != growth || (!state_is(neighbor, block) && !state_is(neighbor, body))) {
        if (block_field_get(block, FIELD_GROWING_PLANT_BLOCK_SCHEDULE_FLUID_TICKS)) {
            level_unsupported(lv, "fluids");
        }
        return st;
    }
    return growing_plant_head_update_body_after_converted_from_head(st, block_default(body));
}
