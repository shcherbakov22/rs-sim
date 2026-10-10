// ScaffoldingBlock: distance 0 on a sturdy block, else one more than the nearest scaffolding beside it; 7 falls

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"
#include "items/block-place-context.h"

int32_t scaffolding_get_distance(level & lv, block_pos pos) {
    block_pos below       = pos_relative(pos, DIRECTION_DOWN);
    state_id  below_state = level_get_state(lv, below);
    int32_t   distance    = 7;
    if (state_is(below_state, BLOCK_SCAFFOLDING)) {
        distance = state_get(below_state, PROP_STABILITY_DISTANCE);
    } else if (block_is_face_sturdy(lv, below_state, below, DIRECTION_UP, SUPPORT_TYPE_FULL)) {
        return 0;
    }
    for (direction dir : DIRECTION_HORIZONTAL) {
        state_id neighbor = level_get_state(lv, pos_relative(pos, dir));
        if (state_is(neighbor, BLOCK_SCAFFOLDING)) {
            distance = std::min(distance, state_get(neighbor, PROP_STABILITY_DISTANCE) + 1);
            if (distance == 1) {
                break;
            }
        }
    }
    return distance;
}

static bool scaffolding_is_bottom(level & lv, state_id st, block_pos pos, int32_t distance) {
    return distance > 0 && !state_is(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)), state_block(st));
}

void scaffolding_on_place(level & lv, state_id st, block_pos pos) {
    level_schedule_tick(lv, pos, state_block(st), 1);
}

void scaffolding_update_shape(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
    }
    level_schedule_tick(lv, pos, state_block(st), 1);
}

// FallingBlockEntity.fall leaves the fluid of the block behind, then the entity spawns
void scaffolding_tick(level & lv, state_id st, block_pos pos) {
    int32_t  distance = scaffolding_get_distance(lv, pos);
    state_id next     = state_set(state_set(st, PROP_STABILITY_DISTANCE, distance), PROP_BOTTOM,
                                  scaffolding_is_bottom(lv, st, pos, distance));
    if (distance == 7) {
        if (state_get(st, PROP_STABILITY_DISTANCE) == 7) {
            if (state_get(st, PROP_WATERLOGGED)) {
                level_unsupported(lv, "fluids");
                return;
            }
            level_set_block(lv, pos, STATE_AIR, UPDATE_FLAG_ALL);
            trace_boundary(lv.tr, "spawn falling_block %d %d %d", pos.x, pos.y, pos.z);
        } else {
            level_destroy_block(lv, pos, true);
        }
    } else if (st != next) {
        level_set_block(lv, pos, next, UPDATE_FLAG_ALL);
    }
}

bool scaffolding_can_survive(level & lv, block_pos pos) {
    return scaffolding_get_distance(lv, pos) < 7;
}

bool scaffolding_can_be_replaced(state_id st, const block_place_context & ctx) {
    return item_stack_get_item(ctx.hit.stack) == block_defs[state_block(st)].as_item;
}

state_id scaffolding_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    block_pos pos      = block_place_context_get_clicked_pos(ctx);
    bool      water    = block_place_context_is_water_source(lv, ctx);
    int32_t   distance = scaffolding_get_distance(lv, pos);
    state_id  st =
        state_set(state_set(block_default(block), PROP_WATERLOGGED, water), PROP_STABILITY_DISTANCE, distance);
    return state_set(st, PROP_BOTTOM, scaffolding_is_bottom(lv, st, pos, distance));
}
