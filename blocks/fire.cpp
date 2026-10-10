// FireBlock: fire burns on a sturdy top face or next to a flammable block; its sides show where it climbs

#include "blocks/handlers.h"
#include "core/rng.h"
#include "engine/level.h"

// FireBlock.canBurn
static bool fire_can_burn(state_id st) {
    return state_has(st, STATE_FLAG_FIRE_CAN_BURN);
}

// FireBlock.PROPERTY_BY_DIRECTION: every direction but down
static prop_id fire_property_by_direction(direction dir) {
    switch (dir) {
        case DIRECTION_NORTH:
            return PROP_NORTH;
        case DIRECTION_EAST:
            return PROP_EAST;
        case DIRECTION_SOUTH:
            return PROP_SOUTH;
        case DIRECTION_WEST:
            return PROP_WEST;
        default:
            return PROP_UP;
    }
}

// FireBlock.getStateForPlacement(BlockGetter, BlockPos)
state_id fire_get_state_for_placement(level & lv, block_pos pos) {
    block_pos below_pos = pos_relative(pos, DIRECTION_DOWN);
    state_id  below     = level_get_state(lv, below_pos);
    state_id  result    = block_default(BLOCK_FIRE);
    if (fire_can_burn(below) || block_is_face_sturdy(lv, below, below_pos, DIRECTION_UP, SUPPORT_TYPE_FULL)) {
        return result;
    }
    for (int32_t d = 0; d < 6; d++) {
        if (d != DIRECTION_DOWN) {
            bool burn = fire_can_burn(level_get_state(lv, pos_relative(pos, (direction) d)));
            result    = state_set(result, fire_property_by_direction((direction) d), burn);
        }
    }
    return result;
}

static state_id fire_get_state_with_age(level & lv, block_pos pos, int32_t age) {
    state_id st = base_fire_get_state(lv, pos);
    return state_is(st, BLOCK_FIRE) ? state_set(st, PROP_AGE_15, age) : st;
}

static bool fire_is_valid_fire_location(level & lv, block_pos pos) {
    for (int32_t d = 0; d < 6; d++) {
        if (fire_can_burn(level_get_state(lv, pos_relative(pos, (direction) d)))) {
            return true;
        }
    }
    return false;
}

bool fire_can_survive(level & lv, block_pos pos) {
    block_pos below_pos = pos_relative(pos, DIRECTION_DOWN);
    return block_is_face_sturdy(lv, level_get_state(lv, below_pos), below_pos, DIRECTION_UP, SUPPORT_TYPE_FULL) ||
           fire_is_valid_fire_location(lv, pos);
}

state_id fire_update_shape(level & lv, state_id st, block_pos pos) {
    return fire_can_survive(lv, pos) ? fire_get_state_with_age(lv, pos, state_get(st, PROP_AGE_15)) : STATE_AIR;
}

static int32_t fire_get_fire_tick_delay(rng & random) {
    return 30 + rng_next_int(random, 10);
}

// the tick is scheduled even when BaseFireBlock.onPlace removed the fire
void fire_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    base_fire_on_place(lv, st, pos, old);
    level_schedule_tick(lv, pos, BLOCK_FIRE, fire_get_fire_tick_delay(lv.random));
}

// ServerLevel.canSpreadFireAround is false with no player near, and the oracle level has none: fire neither spreads nor burns out
void fire_tick(level & lv, block_pos pos) {
    level_schedule_tick(lv, pos, BLOCK_FIRE, fire_get_fire_tick_delay(lv.random));
}
