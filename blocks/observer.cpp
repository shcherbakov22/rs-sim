// ObserverBlock

#include "blocks/handlers.h"
#include "engine/level.h"

static direction observer_facing(state_id st) {
    return (direction) state_get(st, PROP_FACING);
}

static void observer_update_neighbors_in_front(level & lv, state_id st, block_pos pos) {
    direction dir          = observer_facing(st);
    block_pos opposite_pos = pos_relative(pos, direction_opposite(dir));
    level_neighbor_changed(lv, opposite_pos, state_block(st));
    level_update_neighbors_at_except_from_facing(lv, opposite_pos, state_block(st), dir);
}

void observer_tick(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_POWERED)) {
        level_set_block(lv, pos, state_set(st, PROP_POWERED, 0), UPDATE_FLAG_CLIENTS);
    } else {
        level_set_block(lv, pos, state_set(st, PROP_POWERED, 1), UPDATE_FLAG_CLIENTS);
        level_schedule_tick(lv, pos, state_block(st), 2);
    }
    observer_update_neighbors_in_front(lv, st, pos);
}

// ObserverBlock.startSignal
state_id observer_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    int32_t block = state_block(st);
    if (observer_facing(st) == dir && !state_get(st, PROP_POWERED) &&
        !level_ticks_has_scheduled(lv.block_ticks, pos, block)) {
        level_schedule_tick(lv, pos, block, 2);
    }
    return st;
}

int32_t observer_get_signal(state_id st, direction dir) {
    return state_get(st, PROP_POWERED) && observer_facing(st) == dir ? 15 : 0;
}

// a powered observer placed without its pending tick turns off
void observer_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    int32_t block = state_block(st);
    if (state_is(old, block) || !state_get(st, PROP_POWERED) || level_ticks_has_scheduled(lv.block_ticks, pos, block)) {
        return;
    }
    state_id st_new = state_set(st, PROP_POWERED, 0);
    level_set_block(lv, pos, st_new, UPDATE_FLAG_CLIENTS | UPDATE_FLAG_KNOWN_SHAPE);
    observer_update_neighbors_in_front(lv, st_new, pos);
}

void observer_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_POWERED) && level_ticks_has_scheduled(lv.block_ticks, pos, state_block(st))) {
        observer_update_neighbors_in_front(lv, state_set(st, PROP_POWERED, 0), pos);
    }
}
