#include "engine/level-ticks.h"

#include <algorithm>
#include <tuple>

static bool tick_contains(const std::vector<scheduled_tick> & ticks, block_pos pos, int32_t type) {
    return std::any_of(ticks.begin(), ticks.end(),
                       [&](const scheduled_tick & t) { return t.type == type && t.pos == pos; });
}

// ScheduledTick.DRAIN_ORDER
static bool tick_runs_before(const scheduled_tick & a, const scheduled_tick & b) {
    return std::tie(a.trigger_tick, a.priority, a.sub_tick_order) <
           std::tie(b.trigger_tick, b.priority, b.sub_tick_order);
}

bool level_ticks_has_scheduled(const level_ticks & lt, block_pos pos, int32_t type) {
    return tick_contains(lt.pending, pos, type);
}

bool level_ticks_will_tick_this_tick(const level_ticks & lt, block_pos pos, int32_t type) {
    return tick_contains(lt.to_run, pos, type);
}

// LevelChunkTicks.schedule drops a tick for a position and type that already has one
void level_ticks_schedule(level_ticks & lt, const scheduled_tick & tick) {
    if (!tick_contains(lt.pending, tick.pos, tick.type)) {
        lt.pending.insert(std::lower_bound(lt.pending.begin(), lt.pending.end(), tick, tick_runs_before), tick);
    }
}

void level_ticks_collect(level_ticks & lt, int64_t current_tick) {
    auto due = std::find_if(lt.pending.begin(), lt.pending.end(),
                            [&](const scheduled_tick & t) { return t.trigger_tick > current_tick; });
    lt.to_run.assign(lt.pending.begin(), due);
    lt.pending.erase(lt.pending.begin(), due);
}

bool level_ticks_poll(level_ticks & lt, scheduled_tick & out) {
    if (lt.to_run.empty()) return false;
    out = lt.to_run.front();
    lt.to_run.erase(lt.to_run.begin());
    return true;
}
