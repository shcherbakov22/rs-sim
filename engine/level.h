#pragma once

#include "core/common.h"
#include "core/direction.h"
#include "core/rng.h"
#include "core/state.h"
#include "engine/block-entity.h"
#include "engine/level-ticks.h"
#include "engine/neighbor-updater.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// World position of the case origin in the oracle (TmcOracle.BATCH_ORIGIN). Sim positions are relative to it,
// but vanilla hash orders depend on world positions.
constexpr block_pos LEVEL_ORIGIN = { 0, 64, 0 };
// sections keyed by relative positions are the world's sections only while the origin is 16-aligned
static_assert(LEVEL_ORIGIN.x % 16 == 0 && LEVEL_ORIGIN.y % 16 == 0 && LEVEL_ORIGIN.z % 16 == 0);

// Build height relative to the case origin (overworld -64..319).
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

// RedstoneTorchBlock.Toggle
struct redstone_torch_toggle {
    block_pos pos;
    int64_t   when;
};

// One world (ServerLevel). Everything outside the stored sections is air.
struct level {
    std::unordered_map<int64_t, std::unique_ptr<level_section>> sections;

    int64_t                  game_time      = 0;  // relative to the case placement; case data is relative too
    int64_t                  sub_tick_count = 0;
    rng                      random;
    neighbor_updater         updater;
    level_ticks              block_ticks;
    std::vector<block_event> block_events;  // insertion-ordered set
    bool handling_tick = false;             // ServerLevel.handlingTick: from tick start to the end of block events

    // Block entities are never freed during a run: vanilla keeps using removed ones it still holds.
    std::vector<std::unique_ptr<block_entity>>         block_entity_store;
    std::unordered_map<int64_t, block_entity *>        block_entities;  // LevelChunk.blockEntities
    std::vector<std::unique_ptr<block_entity_ticker>>  block_entity_ticker_store;
    std::unordered_map<int64_t, block_entity_ticker *> tickers_in_level;
    std::vector<block_entity_ticker *>                 block_entity_tickers;
    std::vector<block_entity_ticker *>                 pending_block_entity_tickers;
    bool                                               ticking_block_entities = false;

    // RedStoneWireBlock.shouldSignal lives on the block object, RECENT_TOGGLES in a static map keyed by level
    bool                               red_stone_wire_should_signal = true;
    std::vector<redstone_torch_toggle> redstone_torch_recent_toggles;
    trace *                            tr = nullptr;
    std::string                        unsupported;  // first unsupported mechanic hit; the run stops at the next check
    size_t                             unsupported_line = 0;  // trace lines before it, all still exact
};

void level_unsupported(level & lv, const char * fmt, ...) __attribute__((format(printf, 2, 3)));

state_id level_get_state(const level & lv, block_pos pos);

// Writes the state only: no updates, no block entity changes (case placement).
void level_set_state_raw(level & lv, block_pos pos, state_id st);

bool level_set_block(level & lv, block_pos pos, state_id st, int32_t flags, int32_t limit = UPDATE_LIMIT);
bool level_remove_block(level & lv, block_pos pos, bool moved_by_piston);
bool level_destroy_block(level & lv, block_pos pos, bool drop, int32_t limit = UPDATE_LIMIT);

// a new block entity owned by the level, not yet in the world
block_entity * level_new_block_entity(level & lv, block_entity_type type, block_pos pos, state_id st);
// Level.getBlockEntity
block_entity * level_get_block_entity(level & lv, block_pos pos);
block_entity * level_get_block_entity_of_type(level & lv, block_pos pos, int32_t type);
// Level.setBlockEntity (LevelChunk.addAndRegisterBlockEntity)
void           level_set_block_entity(level & lv, block_entity * be);
void           level_remove_block_entity(level & lv, block_pos pos);

// Block.dropResources without a tool
void level_pop_resource(level & lv, block_pos pos);
void level_drop_resources(level & lv, state_id st, block_pos pos, const block_entity * be);

void level_update_neighbors_at(level & lv, block_pos pos, int32_t block);
// updateNeighborsAt for each of the six neighbors, in Direction order
void level_update_neighbors_around(level & lv, block_pos pos, int32_t block);
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

// SignalGetter
int32_t level_get_direct_signal(level & lv, block_pos pos, direction dir);
int32_t level_get_direct_signal_to(level & lv, block_pos pos);
int32_t level_get_control_input_signal(level & lv, block_pos pos, direction dir, bool only_diodes);
bool    level_has_signal(level & lv, block_pos pos, direction dir);
int32_t level_get_signal(level & lv, block_pos pos, direction dir);
bool    level_has_neighbor_signal(level & lv, block_pos pos);
int32_t level_get_best_neighbor_signal(level & lv, block_pos pos);

// ServerLevel.tick
void level_tick(level & lv);
