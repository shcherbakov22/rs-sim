// ComparatorBlock

#include "blocks/handlers.h"
#include "engine/level.h"

static direction comparator_facing(state_id st) {
    return (direction) state_get(st, PROP_HORIZONTAL_FACING);
}

static bool comparator_is_subtract(state_id st) {
    return state_get(st, PROP_MODE_COMPARATOR) == COMPARATOR_MODE_SUBTRACT;
}

state_id comparator_update_shape(level & lv, state_id st, direction dir, block_pos neighbor_pos, state_id neighbor) {
    if (dir == DIRECTION_DOWN && !diode_can_survive_on(lv, neighbor_pos, neighbor)) {
        return STATE_AIR;
    }
    return st;
}

int32_t comparator_get_output_signal(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    return be != nullptr && be->type == BLOCK_ENTITY_TYPE_COMPARATOR ? be->output_signal : 0;
}

// item frames are entities, which the sim does not have, so getItemFrame is always null
static int32_t comparator_get_input_signal(level & lv, state_id st, block_pos pos) {
    int32_t   result    = diode_get_input_signal(lv, st, pos);
    direction dir       = comparator_facing(st);
    block_pos target    = pos_relative(pos, dir);
    state_id  target_st = level_get_state(lv, target);
    if (state_has(target_st, STATE_FLAG_ANALOG_OUTPUT)) {
        return block_get_analog_output_signal(lv, target_st, target, direction_opposite(dir));
    }
    if (result < 15 && state_has(target_st, STATE_FLAG_CONDUCTOR)) {
        target    = pos_relative(target, dir);
        target_st = level_get_state(lv, target);
        if (state_has(target_st, STATE_FLAG_ANALOG_OUTPUT)) {
            return block_get_analog_output_signal(lv, target_st, target, direction_opposite(dir));
        }
    }
    return result;
}

static int32_t comparator_calculate_output_signal(level & lv, state_id st, block_pos pos) {
    int32_t input = comparator_get_input_signal(lv, st, pos);
    if (input == 0) {
        return 0;
    }
    int32_t alternate = diode_get_alternate_signal(lv, st, pos);
    if (alternate > input) {
        return 0;
    }
    return comparator_is_subtract(st) ? input - alternate : input;
}

bool comparator_should_turn_on(level & lv, state_id st, block_pos pos) {
    int32_t input = comparator_get_input_signal(lv, st, pos);
    if (input == 0) {
        return false;
    }
    int32_t side = diode_get_alternate_signal(lv, st, pos);
    return input > side || (input == side && !comparator_is_subtract(st));
}

static void comparator_refresh_output_state(level & lv, state_id st, block_pos pos) {
    int32_t        output = comparator_calculate_output_signal(lv, st, pos);
    block_entity * be     = level_get_block_entity(lv, pos);
    int32_t        old    = 0;
    if (be != nullptr && be->type == BLOCK_ENTITY_TYPE_COMPARATOR) {
        old               = be->output_signal;
        be->output_signal = output;
    }
    if (old != output || !comparator_is_subtract(st)) {
        bool source_on = comparator_should_turn_on(lv, st, pos);
        bool on        = state_get(st, PROP_POWERED);
        if (on && !source_on) {
            level_set_block(lv, pos, state_set(st, PROP_POWERED, 0), UPDATE_FLAG_CLIENTS);
        } else if (!on && source_on) {
            level_set_block(lv, pos, state_set(st, PROP_POWERED, 1), UPDATE_FLAG_CLIENTS);
        }
        diode_update_neighbors_in_front(lv, st, pos);
    }
}

// the click sound draws from the thread-safe random, not the level random
interaction_result comparator_use_without_item(level & lv, state_id st, block_pos pos) {
    st = state_cycle(st, PROP_MODE_COMPARATOR);
    level_set_block(lv, pos, st, UPDATE_FLAG_CLIENTS);
    comparator_refresh_output_state(lv, st, pos);
    return INTERACTION_RESULT_SUCCESS;
}

void comparator_check_tick_on_neighbor(level & lv, state_id st, block_pos pos) {
    int32_t block = state_block(st);
    if (level_ticks_will_tick_this_tick(lv.block_ticks, pos, block)) {
        return;
    }
    int32_t output = comparator_calculate_output_signal(lv, st, pos);
    if (output != comparator_get_output_signal(lv, pos) ||
        (state_get(st, PROP_POWERED) != 0) != comparator_should_turn_on(lv, st, pos)) {
        int32_t priority = diode_should_prioritize(lv, st, pos) ? TICK_PRIORITY_HIGH : TICK_PRIORITY_NORMAL;
        level_schedule_tick(lv, pos, block, 2, priority);
    }
}

void comparator_tick(level & lv, state_id st, block_pos pos) {
    comparator_refresh_output_state(lv, st, pos);
}
