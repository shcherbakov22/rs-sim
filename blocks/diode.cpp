// Diodes: repeaters, comparators and observers pass a signal forward a few ticks later

#include "blocks/handlers.h"
#include "engine/level.h"

#include <algorithm>

// DiodeBlock.updateNeighborsInFront and ObserverBlock's: the block behind the output side and its neighbors
static void diode_update_neighbors_in_front(level & lv, int32_t block, block_pos pos, direction facing) {
    block_pos opposite_pos = pos_relative(pos, direction_opposite(facing));
    level_neighbor_changed(lv, opposite_pos, block);
    level_update_neighbors_at_except_from_facing(lv, opposite_pos, block, facing);
}

// DiodeBlock; RepeaterBlock and ComparatorBlock override parts of it

static bool diode_can_survive_on(level & lv, block_pos neighbor_pos, state_id neighbor) {
    return block_is_face_sturdy(lv, neighbor, neighbor_pos, DIRECTION_UP, SUPPORT_TYPE_RIGID);
}

static bool diode_can_survive(level & lv, state_id, block_pos pos) {
    block_pos below = pos_relative(pos, DIRECTION_DOWN);
    return diode_can_survive_on(lv, below, level_get_state(lv, below));
}

// repeaters only take side input from diodes (sideInputDiodesOnly)
static int32_t diode_get_alternate_signal(level & lv, state_id st, block_pos pos) {
    direction dir                    = state_facing(st);
    direction cw                     = direction_clockwise(dir);
    direction ccw                    = direction_counter_clockwise(dir);
    bool      side_input_diodes_only = state_is(st, BLOCK_REPEATER);
    return std::max(level_get_control_input_signal(lv, pos_relative(pos, cw), cw, side_input_diodes_only),
                    level_get_control_input_signal(lv, pos_relative(pos, ccw), ccw, side_input_diodes_only));
}

// RepeaterBlock.isLocked; only repeaters lock
static bool diode_is_locked(level & lv, state_id st, block_pos pos) {
    return state_is(st, BLOCK_REPEATER) && diode_get_alternate_signal(lv, st, pos) > 0;
}

// RepeaterBlock.getDelay, 2 for comparators
static int32_t diode_get_delay(state_id st) {
    return state_is(st, BLOCK_COMPARATOR) ? 2 : state_get(st, PROP_DELAY) * 2;
}

static int32_t diode_get_input_signal(level & lv, state_id st, block_pos pos) {
    direction dir    = state_facing(st);
    block_pos target = pos_relative(pos, dir);
    int32_t   input  = level_get_signal(lv, target, dir);
    if (input >= 15) return input;
    state_id target_st = level_get_state(lv, target);
    return std::max(input, state_is(target_st, BLOCK_REDSTONE_WIRE) ? state_get(target_st, PROP_POWER) : 0);
}

static bool diode_should_prioritize(level & lv, state_id st, block_pos pos) {
    direction dir      = direction_opposite(state_facing(st));
    state_id  opposite = level_get_state(lv, pos_relative(pos, dir));
    return block_instanceof(state_block(opposite), VCLASS_DIODE_BLOCK) && state_facing(opposite) != dir;
}

// ComparatorBlock

static bool comparator_is_subtract(state_id st) {
    return state_get(st, PROP_MODE_COMPARATOR) == COMPARATOR_MODE_SUBTRACT;
}

static int32_t comparator_get_output_signal(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_COMPARATOR);
    return be != nullptr ? be->output_signal : 0;
}

// item frames are entities, which the sim does not have, so getItemFrame is always null
static int32_t comparator_get_input_signal(level & lv, state_id st, block_pos pos) {
    int32_t   result    = diode_get_input_signal(lv, st, pos);
    direction dir       = state_facing(st);
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
    if (input == 0) return 0;
    int32_t alternate = diode_get_alternate_signal(lv, st, pos);
    if (alternate > input) return 0;
    return comparator_is_subtract(st) ? input - alternate : input;
}

static bool comparator_should_turn_on(level & lv, state_id st, block_pos pos) {
    int32_t input = comparator_get_input_signal(lv, st, pos);
    if (input == 0) return false;
    int32_t side = diode_get_alternate_signal(lv, st, pos);
    return input > side || (input == side && !comparator_is_subtract(st));
}

// refreshOutputState
static void comparator_tick(level & lv, state_id st, block_pos pos) {
    int32_t        output = comparator_calculate_output_signal(lv, st, pos);
    block_entity * be     = level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_COMPARATOR);
    int32_t        old    = 0;
    if (be != nullptr) {
        old               = be->output_signal;
        be->output_signal = output;
    }
    if (old != output || !comparator_is_subtract(st)) {
        bool source_on = comparator_should_turn_on(lv, st, pos);
        if (source_on != (bool) state_get(st, PROP_POWERED)) {
            level_set_block(lv, pos, state_set(st, PROP_POWERED, source_on), UPDATE_FLAG_CLIENTS);
        }
        diode_update_neighbors_in_front(lv, state_block(st), pos, state_facing(st));
    }
}

static void comparator_check_tick_on_neighbor(level & lv, state_id st, block_pos pos) {
    int32_t block = state_block(st);
    if (level_ticks_will_tick_this_tick(lv.block_ticks, pos, block)) return;
    int32_t output = comparator_calculate_output_signal(lv, st, pos);
    if (output != comparator_get_output_signal(lv, pos) ||
        (state_get(st, PROP_POWERED) != 0) != comparator_should_turn_on(lv, st, pos)) {
        int32_t priority = diode_should_prioritize(lv, st, pos) ? TICK_PRIORITY_HIGH : TICK_PRIORITY_NORMAL;
        level_schedule_tick(lv, pos, block, 2, priority);
    }
}

static state_id comparator_update_shape(level &  lv,
                                        state_id st,
                                        block_pos,
                                        direction dir,
                                        block_pos neighbor_pos,
                                        state_id  neighbor) {
    return dir == DIRECTION_DOWN && !diode_can_survive_on(lv, neighbor_pos, neighbor) ? STATE_AIR : st;
}

// DiodeBlock again, with the overrides of its subclasses

static bool diode_should_turn_on(level & lv, state_id st, block_pos pos) {
    if (state_is(st, BLOCK_COMPARATOR)) return comparator_should_turn_on(lv, st, pos);
    return diode_get_input_signal(lv, st, pos) > 0;
}

static void diode_tick(level & lv, state_id st, block_pos pos) {
    if (diode_is_locked(lv, st, pos)) return;
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

// getOutputSignal is 15 for repeaters; getDirectSignal is the same
static int32_t diode_get_signal(level & lv, state_id st, block_pos pos, direction dir) {
    if (!state_get(st, PROP_POWERED) || state_facing(st) != dir) return 0;
    return state_is(st, BLOCK_COMPARATOR) ? comparator_get_output_signal(lv, pos) : 15;
}

static void diode_check_tick_on_neighbor(level & lv, state_id st, block_pos pos) {
    if (state_is(st, BLOCK_COMPARATOR)) {
        comparator_check_tick_on_neighbor(lv, st, pos);
        return;
    }
    if (diode_is_locked(lv, st, pos)) return;
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

static void diode_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    if (block_can_survive(lv, st, pos)) {
        diode_check_tick_on_neighbor(lv, st, pos);
        return;
    }
    level_drop_resources(lv, st, pos,
                         state_has(st, STATE_FLAG_BLOCK_ENTITY) ? level_get_block_entity(lv, pos) : nullptr);
    level_remove_block(lv, pos, false);
    level_update_neighbors_around(lv, pos, state_block(st));
}

static void diode_on_place(level & lv, state_id st, block_pos pos, state_id, bool) {
    diode_update_neighbors_in_front(lv, state_block(st), pos, state_facing(st));
}

static void diode_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (!moved_by_piston) diode_update_neighbors_in_front(lv, state_block(st), pos, state_facing(st));
}

// RepeaterBlock: a powered diode beside it locks it
static state_id repeater_update_shape(level &   lv,
                                      state_id  st,
                                      block_pos pos,
                                      direction dir,
                                      block_pos neighbor_pos,
                                      state_id  neighbor) {
    if (dir == DIRECTION_DOWN && !diode_can_survive_on(lv, neighbor_pos, neighbor)) return STATE_AIR;
    if (direction_get_axis(dir) != direction_get_axis(state_facing(st))) {
        return state_set(st, PROP_LOCKED, diode_is_locked(lv, st, pos));
    }
    return st;
}

// ObserverBlock: a shape change in front sends a 2 tick pulse out the back

static void observer_tick(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_POWERED)) {
        level_set_block(lv, pos, state_set(st, PROP_POWERED, 0), UPDATE_FLAG_CLIENTS);
    } else {
        level_set_block(lv, pos, state_set(st, PROP_POWERED, 1), UPDATE_FLAG_CLIENTS);
        level_schedule_tick(lv, pos, state_block(st), 2);
    }
    diode_update_neighbors_in_front(lv, state_block(st), pos, state_facing(st));
}

// startSignal
static state_id observer_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos, state_id) {
    int32_t block = state_block(st);
    if (state_facing(st) == dir && !state_get(st, PROP_POWERED) &&
        !level_ticks_has_scheduled(lv.block_ticks, pos, block)) {
        level_schedule_tick(lv, pos, block, 2);
    }
    return st;
}

// getDirectSignal is the same
static int32_t observer_get_signal(level &, state_id st, block_pos, direction dir) {
    return state_get(st, PROP_POWERED) && state_facing(st) == dir ? 15 : 0;
}

// a powered observer placed without its pending tick turns off
static void observer_on_place(level & lv, state_id st, block_pos pos, state_id old, bool) {
    int32_t block = state_block(st);
    if (state_is(old, block) || !state_get(st, PROP_POWERED) || level_ticks_has_scheduled(lv.block_ticks, pos, block)) {
        return;
    }
    level_set_block(lv, pos, state_set(st, PROP_POWERED, 0), UPDATE_FLAG_CLIENTS | UPDATE_FLAG_KNOWN_SHAPE);
    diode_update_neighbors_in_front(lv, block, pos, state_facing(st));
}

static void observer_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool) {
    if (state_get(st, PROP_POWERED) && level_ticks_has_scheduled(lv.block_ticks, pos, state_block(st))) {
        diode_update_neighbors_in_front(lv, state_block(st), pos, state_facing(st));
    }
}

// clang-format off
const std::vector<block_hooks> diode_hooks = {
    { .vclass = VCLASS_DIODE_BLOCK,
      .neighbor_changed = diode_neighbor_changed,
      .on_place = diode_on_place,
      .affect_neighbors_after_removal = diode_affect_neighbors_after_removal,
      .tick = diode_tick,
      .get_signal = diode_get_signal,
      .get_direct_signal = diode_get_signal,
      .can_survive = diode_can_survive },
    { .vclass = VCLASS_REPEATER_BLOCK, .update_shape = repeater_update_shape },
    { .vclass = VCLASS_COMPARATOR_BLOCK,
      .update_shape = comparator_update_shape,
      .tick = comparator_tick,
      .trigger_event = base_entity_block_trigger_event },
    { .vclass = VCLASS_OBSERVER_BLOCK,
      .update_shape = observer_update_shape,
      .on_place = observer_on_place,
      .affect_neighbors_after_removal = observer_affect_neighbors_after_removal,
      .tick = observer_tick,
      .get_signal = observer_get_signal,
      .get_direct_signal = observer_get_signal },
};
// clang-format on
