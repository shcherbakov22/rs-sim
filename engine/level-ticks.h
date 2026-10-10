#pragma once

#include "core/common.h"

#include <string_view>
#include <vector>

// vanilla TickPriority values
enum tick_priority {
    TICK_PRIORITY_EXTREMELY_HIGH = -3,
    TICK_PRIORITY_VERY_HIGH      = -2,
    TICK_PRIORITY_HIGH           = -1,
    TICK_PRIORITY_NORMAL         = 0,
    TICK_PRIORITY_LOW            = 1,
    TICK_PRIORITY_VERY_LOW       = 2,
    TICK_PRIORITY_EXTREMELY_LOW  = 3,
};

const char * tick_priority_name(int32_t priority);
bool         tick_priority_parse(std::string_view name, int32_t & out);

struct scheduled_tick {
    int32_t   block;
    block_pos pos;
    int64_t   trigger_tick;
    int32_t   priority;
    int64_t   sub_tick_order;
};

// LevelTicks<Block>. Vanilla keeps one queue per chunk and merges them by priority and sub-tick, ignoring the
// trigger tick. Every delay is >= 1, so all ticks due in one game tick share their trigger tick and the merge is
// the same as one list sorted by ScheduledTick.DRAIN_ORDER.
struct level_ticks {
    std::vector<scheduled_tick> pending;  // DRAIN_ORDER, front runs first
    std::vector<scheduled_tick> to_run;   // this tick's ticks; front runs next
};

bool level_ticks_has_scheduled(const level_ticks & lt, block_pos pos, int32_t block);
bool level_ticks_will_tick_this_tick(const level_ticks & lt, block_pos pos, int32_t block);
void level_ticks_schedule(level_ticks & lt, const scheduled_tick & tick);

// moves the due ticks to to_run; then call level_ticks_poll until it returns false
void level_ticks_collect(level_ticks & lt, int64_t current_tick);
bool level_ticks_poll(level_ticks & lt, scheduled_tick & out);
