#pragma once

#include "core/common.h"
#include "core/direction.h"
#include "core/rng.h"
#include "core/state.h"
#include "engine/level-ticks.h"
#include "engine/neighbor-updater.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Build height relative to the case origin (overworld -64..319, oracle origin at y 64).
constexpr int32_t LEVEL_Y_MIN = -128;
constexpr int32_t LEVEL_Y_MAX = 255;

// Block.UPDATE_*
enum update_flag {
    UPDATE_FLAG_NEIGHBORS                     = 1,
    UPDATE_FLAG_CLIENTS                       = 2,
    UPDATE_FLAG_INVISIBLE                     = 4,
    UPDATE_FLAG_IMMEDIATE                     = 8,
    UPDATE_FLAG_KNOWN_SHAPE                   = 16,
    UPDATE_FLAG_SUPPRESS_DROPS                = 32,
    UPDATE_FLAG_MOVE_BY_PISTON                = 64,
    UPDATE_FLAG_SKIP_SHAPE_UPDATE_ON_WIRE     = 128,
    UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS = 256,
    UPDATE_FLAG_SKIP_ON_PLACE                 = 512,
    UPDATE_FLAG_ALL                           = 3,
    UPDATE_FLAG_ALL_IMMEDIATE                 = 11,
};

constexpr int32_t UPDATE_LIMIT = 512;

struct level_section {
    state_id states[16 * 16 * 16] = {};
    int32_t  n_non_air            = 0;
};

struct block_event {
    block_pos pos;
    int32_t   block;
    int32_t   param_a;
    int32_t   param_b;
};

// One world (ServerLevel). Everything outside the stored sections is air.
struct level {
    std::unordered_map<int64_t, std::unique_ptr<level_section>> sections;

    int64_t                  game_time      = 0;  // relative to the case placement; case data is relative too
    int64_t                  sub_tick_count = 0;
    rng                      random;
    neighbor_updater         updater;
    level_ticks              block_ticks;
    std::vector<block_event> block_events;          // insertion-ordered set
    trace *                  tr = nullptr;
    std::string              unsupported;           // first unsupported mechanic hit; the run stops at the next check
    size_t                   unsupported_line = 0;  // trace lines before it, all still exact
};

void level_unsupported(level & lv, const char * fmt, ...) __attribute__((format(printf, 2, 3)));

state_id level_get_state(const level & lv, block_pos pos);

// Writes the state only: no updates, no block entity changes (case placement).
void level_set_state_raw(level & lv, block_pos pos, state_id st);

bool level_set_block(level & lv, block_pos pos, state_id st, int32_t flags, int32_t limit = UPDATE_LIMIT);
bool level_remove_block(level & lv, block_pos pos, bool moved_by_piston);
bool level_destroy_block(level & lv, block_pos pos, bool drop, int32_t limit = UPDATE_LIMIT);

void level_update_neighbors_at(level & lv, block_pos pos, int32_t block);
void level_update_neighbors_at_except_from_facing(level & lv, block_pos pos, int32_t block, direction skip);
void level_neighbor_changed(level & lv, block_pos pos, int32_t block);
void level_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block, bool moved_by_piston);
void level_neighbor_shape_changed(level &   lv,
                                  direction dir,
                                  block_pos pos,
                                  block_pos neighbor_pos,
                                  state_id  neighbor,
                                  int32_t   flags,
                                  int32_t   limit);
void level_update_neighbour_for_output_signal(level & lv, block_pos pos, int32_t block);

void level_schedule_tick(level &   lv,
                         block_pos pos,
                         int32_t   block,
                         int32_t   delay,
                         int32_t   priority = TICK_PRIORITY_NORMAL);
void level_block_event(level & lv, block_pos pos, int32_t block, int32_t param_a, int32_t param_b);

// ServerLevel.tick
void level_tick(level & lv);
