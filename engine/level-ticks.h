#pragma once

#include "core/common.h"
#include "gen/blocks.h"

#include <vector>

struct scheduled_tick {
    int32_t   type;  // block, or fluid
    block_pos pos;
    int64_t   trigger_tick;
    int32_t   priority;
    int64_t   sub_tick_order;
};

// LevelTicks<Block> and LevelTicks<Fluid>. Vanilla merges its per-chunk queues by priority and sub-tick.
// Every delay is >= 1, so the due ticks share their trigger tick and one list in ScheduledTick.DRAIN_ORDER is the same.
struct level_ticks {
    std::vector<scheduled_tick> pending;  // DRAIN_ORDER, front runs first
    std::vector<scheduled_tick> to_run;   // this tick's ticks; front runs next
};

bool level_ticks_has_scheduled(const level_ticks & lt, block_pos pos, int32_t type);
bool level_ticks_will_tick_this_tick(const level_ticks & lt, block_pos pos, int32_t type);
void level_ticks_schedule(level_ticks & lt, const scheduled_tick & tick);

// moves the due ticks to to_run; then call level_ticks_poll until it returns false
void level_ticks_collect(level_ticks & lt, int64_t current_tick);
bool level_ticks_poll(level_ticks & lt, scheduled_tick & out);
