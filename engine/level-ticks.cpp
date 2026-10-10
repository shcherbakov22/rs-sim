#include "engine/level-ticks.h"

#include <algorithm>
#include <tuple>

static const char * const tick_priority_names[] = {
    "extremely_high", "very_high", "high", "normal", "low", "very_low", "extremely_low",
};

const char * tick_priority_name(int32_t priority) {
    TMC_ASSERT(priority >= -3 && priority <= 3);
    return tick_priority_names[priority + 3];
}

bool tick_priority_parse(std::string_view name, int32_t & out) {
    for (int32_t i = 0; i < 7; i++) {
        if (name == tick_priority_names[i]) {
            out = i - 3;
            return true;
        }
    }
    return false;
}

static bool tick_contains(const std::vector<scheduled_tick> & ticks, block_pos pos, int32_t block) {
    return std::any_of(ticks.begin(), ticks.end(),
                       [&](const scheduled_tick & t) { return t.block == block && t.pos == pos; });
}

// ScheduledTick.DRAIN_ORDER
static bool tick_runs_before(const scheduled_tick & a, const scheduled_tick & b) {
    return std::tie(a.trigger_tick, a.priority, a.sub_tick_order) <
           std::tie(b.trigger_tick, b.priority, b.sub_tick_order);
}

bool level_ticks_has_scheduled(const level_ticks & lt, block_pos pos, int32_t block) {
    return tick_contains(lt.pending, pos, block);
}

bool level_ticks_will_tick_this_tick(const level_ticks & lt, block_pos pos, int32_t block) {
    return tick_contains(lt.to_run, pos, block);
}

// LevelChunkTicks.schedule drops a tick for a position and block that already has one
void level_ticks_schedule(level_ticks & lt, const scheduled_tick & tick) {
    if (!tick_contains(lt.pending, tick.pos, tick.block)) {
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
