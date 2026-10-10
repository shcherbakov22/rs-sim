#pragma once

#include "core/common.h"
#include "core/rng.h"
#include "core/state.h"

#include <memory>
#include <string>
#include <unordered_map>

// Build height relative to the case origin (overworld -64..319, oracle origin at y 64).
constexpr int32_t LEVEL_Y_MIN = -128;
constexpr int32_t LEVEL_Y_MAX = 255;

struct level_section {
    state_id states[16 * 16 * 16] = {};
};

// One world. Everything outside the stored sections is air.
struct level {
    std::unordered_map<int64_t, std::unique_ptr<level_section>> sections;

    int64_t     game_time = 0;  // relative to the case placement; case data stores times relative to it too
    rng         random;
    trace *     tr = nullptr;
    std::string unsupported;  // first unsupported mechanic hit; the run stops at the next check
};

state_id level_get_state(level & lv, block_pos pos);

// Writes the state only: no updates, no block entity changes (case placement).
void level_set_state_raw(level & lv, block_pos pos, state_id st);

void level_unsupported(level & lv, const char * fmt, ...) __attribute__((format(printf, 2, 3)));

// ServerLevel.tick
void level_tick(level & lv);
