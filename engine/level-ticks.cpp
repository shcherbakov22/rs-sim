#include "engine/level-ticks.h"

#include "engine/level.h"

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

// ChunkPos.asLong of the world position
static int64_t chunk_key(block_pos pos) {
    int64_t x = (pos.x + LEVEL_ORIGIN.x) >> 4;
    int64_t z = (pos.z + LEVEL_ORIGIN.z) >> 4;
    return (x & 0xFFFFFFFFLL) | (z & 0xFFFFFFFFLL) << 32;
}

// ScheduledTick.DRAIN_ORDER: < 0 if a runs first
static int32_t tick_compare_drain(const scheduled_tick & a, const scheduled_tick & b) {
    if (a.trigger_tick != b.trigger_tick) {
        return a.trigger_tick < b.trigger_tick ? -1 : 1;
    }
    if (a.priority != b.priority) {
        return a.priority < b.priority ? -1 : 1;
    }
    return a.sub_tick_order < b.sub_tick_order ? -1 : a.sub_tick_order > b.sub_tick_order ? 1 : 0;
}

// ScheduledTick.INTRA_TICK_DRAIN_ORDER
static int32_t tick_compare_intra(const scheduled_tick & a, const scheduled_tick & b) {
    if (a.priority != b.priority) {
        return a.priority < b.priority ? -1 : 1;
    }
    return a.sub_tick_order < b.sub_tick_order ? -1 : a.sub_tick_order > b.sub_tick_order ? 1 : 0;
}

static bool tick_same(const scheduled_tick & t, block_pos pos, int32_t block) {
    return t.block == block && t.pos == pos;
}

static bool tick_equals(const scheduled_tick & a, const scheduled_tick & b) {
    return tick_same(a, b.pos, b.block) && a.trigger_tick == b.trigger_tick && a.priority == b.priority &&
           a.sub_tick_order == b.sub_tick_order;
}

bool level_ticks_has_scheduled(const level_ticks & lt, block_pos pos, int32_t block) {
    auto it = lt.containers.find(chunk_key(pos));
    if (it == lt.containers.end()) {
        return false;
    }
    for (const scheduled_tick & t : it->second.queue) {
        if (tick_same(t, pos, block)) {
            return true;
        }
    }
    return false;
}

bool level_ticks_will_tick_this_tick(const level_ticks & lt, block_pos pos, int32_t block) {
    for (const scheduled_tick & t : lt.to_run) {
        if (tick_same(t, pos, block)) {
            return true;
        }
    }
    return false;
}

void level_ticks_schedule(level_ticks & lt, const scheduled_tick & tick) {
    int64_t             key = chunk_key(tick.pos);
    level_chunk_ticks & c   = lt.containers[key];
    for (const scheduled_tick & t : c.queue) {
        if (tick_same(t, tick.pos, tick.block)) {
            return;
        }
    }
    size_t i = 0;
    while (i < c.queue.size() && tick_compare_drain(c.queue[i], tick) < 0) {
        i++;
    }
    c.queue.insert(c.queue.begin() + i, tick);
    // onTickAdded: a new first tick moves the chunk's next tick
    if (tick_equals(c.queue.front(), tick)) {
        lt.next_tick_for_container[key] = tick.trigger_tick;
    }
}

static const scheduled_tick * container_peek(const level_ticks & lt, int64_t key) {
    auto it = lt.containers.find(key);
    return it == lt.containers.end() || it->second.queue.empty() ? nullptr : &it->second.queue.front();
}

static scheduled_tick container_poll(level_ticks & lt, int64_t key) {
    std::vector<scheduled_tick> & q = lt.containers[key].queue;
    scheduled_tick                t = q.front();
    q.erase(q.begin());
    return t;
}

// containersToTick is a priority queue by each chunk's first tick; the order is total, so a scan is equivalent
static int64_t containers_peek(const level_ticks & lt, size_t & index) {
    index = 0;
    for (size_t i = 1; i < lt.containers_to_tick.size(); i++) {
        if (tick_compare_intra(*container_peek(lt, lt.containers_to_tick[i]),
                               *container_peek(lt, lt.containers_to_tick[index])) < 0) {
            index = i;
        }
    }
    return lt.containers_to_tick[index];
}

static void update_container_scheduling(level_ticks & lt, const scheduled_tick & next) {
    lt.next_tick_for_container[chunk_key(next.pos)] = next.trigger_tick;
}

static bool can_schedule_more(const level_ticks & lt, int32_t max_ticks) {
    return (int32_t) lt.to_run.size() < max_ticks;
}

static void sort_containers_to_tick(level_ticks & lt, int64_t current_tick) {
    for (auto it = lt.next_tick_for_container.begin(); it != lt.next_tick_for_container.end();) {
        if (it->second > current_tick) {
            ++it;
            continue;
        }
        const scheduled_tick * head = container_peek(lt, it->first);
        if (head == nullptr) {
            it = lt.next_tick_for_container.erase(it);
        } else if (head->trigger_tick > current_tick) {
            it->second = head->trigger_tick;
            ++it;
        } else {
            lt.containers_to_tick.push_back(it->first);
            it = lt.next_tick_for_container.erase(it);
        }
    }
}

static void drain_from_current_container(level_ticks & lt, int64_t current, int64_t current_tick, int32_t max_ticks) {
    if (!can_schedule_more(lt, max_ticks)) {
        return;
    }
    const scheduled_tick * next_from_next = nullptr;
    if (!lt.containers_to_tick.empty()) {
        size_t index;
        next_from_next = container_peek(lt, containers_peek(lt, index));
    }
    while (can_schedule_more(lt, max_ticks)) {
        const scheduled_tick * t = container_peek(lt, current);
        if (t == nullptr || t->trigger_tick > current_tick ||
            (next_from_next != nullptr && tick_compare_intra(*t, *next_from_next) > 0)) {
            break;
        }
        lt.to_run.push_back(container_poll(lt, current));
    }
}

void level_ticks_collect(level_ticks & lt, int64_t current_tick, int32_t max_ticks) {
    sort_containers_to_tick(lt, current_tick);
    while (can_schedule_more(lt, max_ticks) && !lt.containers_to_tick.empty()) {
        size_t  index;
        int64_t top = containers_peek(lt, index);
        lt.containers_to_tick.erase(lt.containers_to_tick.begin() + index);
        lt.to_run.push_back(container_poll(lt, top));
        drain_from_current_container(lt, top, current_tick, max_ticks);
        const scheduled_tick * next = container_peek(lt, top);
        if (next != nullptr) {
            if (next->trigger_tick <= current_tick && can_schedule_more(lt, max_ticks)) {
                lt.containers_to_tick.push_back(top);
            } else {
                update_container_scheduling(lt, *next);
            }
        }
    }
    // rescheduleLeftoverContainers
    for (int64_t key : lt.containers_to_tick) {
        update_container_scheduling(lt, *container_peek(lt, key));
    }
}

bool level_ticks_poll(level_ticks & lt, scheduled_tick & out) {
    if (lt.to_run.empty()) {
        return false;
    }
    out = lt.to_run.front();
    lt.to_run.erase(lt.to_run.begin());
    return true;
}

void level_ticks_cleanup(level_ticks & lt) {
    lt.to_run.clear();
    lt.containers_to_tick.clear();
}
