// DiodeBlock. Repeaters are the only diodes: comparators have block entities, which the sim does not place yet.

#include "blocks/handlers.h"
#include "engine/level.h"

#include <algorithm>

static direction diode_facing(state_id st) {
    return (direction) state_get(st, PROP_HORIZONTAL_FACING);
}

bool diode_can_survive_on(level & lv, state_id neighbor) {
    return block_is_face_sturdy(lv, neighbor, DIRECTION_UP, SUPPORT_TYPE_RIGID);
}

bool diode_can_survive(level & lv, block_pos pos) {
    return diode_can_survive_on(lv, level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)));
}

// virtual: RepeaterBlock overrides it
static bool diode_is_locked(level & lv, state_id st, block_pos pos) {
    TMC_ASSERT(state_is(st, BLOCK_REPEATER));
    return repeater_is_locked(lv, st, pos);
}

static int32_t diode_get_delay(state_id st) {
    TMC_ASSERT(state_is(st, BLOCK_REPEATER));
    return repeater_get_delay(st);
}

static int32_t diode_get_input_signal(level & lv, state_id st, block_pos pos) {
    direction dir    = diode_facing(st);
    block_pos target = pos_relative(pos, dir);
    int32_t   input  = level_get_signal(lv, target, dir);
    if (input >= 15) {
        return input;
    }
    state_id target_st = level_get_state(lv, target);
    return std::max(input, state_is(target_st, BLOCK_REDSTONE_WIRE) ? state_get(target_st, PROP_POWER) : 0);
}

static bool diode_should_turn_on(level & lv, state_id st, block_pos pos) {
    return diode_get_input_signal(lv, st, pos) > 0;
}

void diode_tick(level & lv, state_id st, block_pos pos) {
    if (diode_is_locked(lv, st, pos)) {
        return;
    }
    bool on             = state_get(st, PROP_POWERED);
    bool should_turn_on = diode_should_turn_on(lv, st, pos);
    if (on && !should_turn_on) {
        level_set_block(lv, pos, state_set(st, PROP_POWERED, 0), UPDATE_FLAG_CLIENTS);
    } else if (!on) {
        level_set_block(lv, pos, state_set(st, PROP_POWERED, 1), UPDATE_FLAG_CLIENTS);
        if (!should_turn_on) {
            level_schedule_tick(lv, pos, state_block(st), diode_get_delay(st), TICK_PRIORITY_VERY_HIGH);
        }
    }
}

// DiodeBlock.getOutputSignal is 15 for repeaters
int32_t diode_get_signal(state_id st, direction dir) {
    return state_get(st, PROP_POWERED) && diode_facing(st) == dir ? 15 : 0;
}

static bool diode_should_prioritize(level & lv, state_id st, block_pos pos) {
    direction dir      = direction_opposite(diode_facing(st));
    state_id  opposite = level_get_state(lv, pos_relative(pos, dir));
    return block_instanceof(state_block(opposite), VCLASS_DIODE_BLOCK) && diode_facing(opposite) != dir;
}

static void diode_check_tick_on_neighbor(level & lv, state_id st, block_pos pos) {
    if (diode_is_locked(lv, st, pos)) {
        return;
    }
    bool    on    = state_get(st, PROP_POWERED);
    int32_t block = state_block(st);
    if (on != diode_should_turn_on(lv, st, pos) && !level_ticks_will_tick_this_tick(lv.block_ticks, pos, block)) {
        int32_t priority = TICK_PRIORITY_HIGH;
        if (diode_should_prioritize(lv, st, pos)) {
            priority = TICK_PRIORITY_EXTREMELY_HIGH;
        } else if (on) {
            priority = TICK_PRIORITY_VERY_HIGH;
        }
        level_schedule_tick(lv, pos, block, diode_get_delay(st), priority);
    }
}

void diode_neighbor_changed(level & lv, state_id st, block_pos pos) {
    if (block_can_survive(lv, st, pos)) {
        diode_check_tick_on_neighbor(lv, st, pos);
        return;
    }
    level_drop_resources(lv, st, pos);
    level_remove_block(lv, pos, false);
    for (int32_t d = 0; d < 6; d++) {
        level_update_neighbors_at(lv, pos_relative(pos, (direction) d), state_block(st));
    }
}

// repeaters only take side input from diodes (sideInputDiodesOnly)
int32_t diode_get_alternate_signal(level & lv, state_id st, block_pos pos) {
    direction dir = diode_facing(st);
    direction cw  = direction_clockwise(dir);
    direction ccw = direction_counter_clockwise(dir);
    return std::max(level_get_control_input_signal(lv, pos_relative(pos, cw), cw, true),
                    level_get_control_input_signal(lv, pos_relative(pos, ccw), ccw, true));
}

static void diode_update_neighbors_in_front(level & lv, state_id st, block_pos pos) {
    direction dir          = diode_facing(st);
    block_pos opposite_pos = pos_relative(pos, direction_opposite(dir));
    level_neighbor_changed(lv, opposite_pos, state_block(st));
    level_update_neighbors_at_except_from_facing(lv, opposite_pos, state_block(st), dir);
}

void diode_on_place(level & lv, state_id st, block_pos pos) {
    diode_update_neighbors_in_front(lv, st, pos);
}

void diode_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (!moved_by_piston) {
        diode_update_neighbors_in_front(lv, st, pos);
    }
}
