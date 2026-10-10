#pragma once

#include "core/common.h"

#include <string_view>
#include <unordered_map>
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

// LevelChunkTicks: pending ticks of one chunk, sorted by DRAIN_ORDER (front runs first)
struct level_chunk_ticks {
    std::vector<scheduled_tick> queue;
};

// LevelTicks<Block>. Chunks are compared by their first tick in INTRA_TICK_DRAIN_ORDER (no trigger tick),
// so an overdue tick does not sort before other chunks' ticks.
struct level_ticks {
    std::unordered_map<int64_t, level_chunk_ticks> containers;
    std::unordered_map<int64_t, int64_t>           next_tick_for_container;
    std::vector<int64_t>                           containers_to_tick;
    std::vector<scheduled_tick>                    to_run;  // this tick's ticks; front runs next
};

bool level_ticks_has_scheduled(const level_ticks & lt, block_pos pos, int32_t block);
bool level_ticks_will_tick_this_tick(const level_ticks & lt, block_pos pos, int32_t block);
void level_ticks_schedule(level_ticks & lt, const scheduled_tick & tick);

// collects due ticks; then call level_ticks_poll until it returns false, then level_ticks_cleanup
void level_ticks_collect(level_ticks & lt, int64_t current_tick, int32_t max_ticks);
bool level_ticks_poll(level_ticks & lt, scheduled_tick & out);
void level_ticks_cleanup(level_ticks & lt);
