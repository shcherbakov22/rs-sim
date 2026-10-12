#pragma once

#include "core/common.h"
#include "core/direction.h"
#include "core/rng.h"
#include "core/state.h"
#include "engine/block-entity.h"
#include "engine/entity.h"
#include "engine/level-ticks.h"
#include "engine/neighbor-updater.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// World position of the case origin in the oracle (TmcOracle.BATCH_ORIGIN).
// Sim positions are relative to it, but vanilla hash orders depend on world positions.
constexpr block_pos LEVEL_ORIGIN = { 0, 64, 0 };
// sections keyed by relative positions are the world's sections only while the origin is 16-aligned
static_assert(LEVEL_ORIGIN.x % 16 == 0 && LEVEL_ORIGIN.y % 16 == 0 && LEVEL_ORIGIN.z % 16 == 0);

// SectionPos.asLong of section coordinates
inline int64_t section_pos_as_long(int32_t x, int32_t y, int32_t z) {
    return ((int64_t) x & 0x3FFFFF) << 42 | ((int64_t) z & 0x3FFFFF) << 20 | ((int64_t) y & 0xFFFFF);
}

// BlockPos.asLong
inline int64_t pos_key(block_pos pos) {
    return ((int64_t) pos.x & 0x3FFFFFF) << 38 | ((int64_t) pos.z & 0x3FFFFFF) << 12 | ((int64_t) pos.y & 0xFFF);
}

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

// sections per axis of the grid around the origin that level_find_section reads without the map
constexpr int32_t LEVEL_NEAR_SECTIONS = 8;

struct level_section {
    state_id states[16 * 16 * 16] = {};
    int32_t  n_non_air            = 0;
    uint32_t version              = 0;  // grows with every write, so an unchanged version means unchanged states
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
    std::unordered_map<int64_t, std::unique_ptr<level_section>> sections;  // never removed
    // the sections within LEVEL_NEAR_SECTIONS / 2 of the origin (null: none there); cases stay near it
    level_section * near_sections[LEVEL_NEAR_SECTIONS * LEVEL_NEAR_SECTIONS * LEVEL_NEAR_SECTIONS] = {};

    int64_t                  game_time      = 0;  // relative to the case placement; case data is relative too
    int64_t                  sub_tick_count = 0;
    rng                      random;
    neighbor_updater         updater;
    level_ticks              block_ticks;
    level_ticks              fluid_ticks;
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
    // grow when a block entity joins or leaves the level, and with every block_entity.mod_count; sleeping hoppers watch them
    uint64_t                                           block_entities_version = 0;
    uint64_t                                           item_writes            = 0;

    // RedStoneWireBlock.shouldSignal lives on the block object, RECENT_TOGGLES in a static map keyed by level
    bool                               red_stone_wire_should_signal = true;
    std::vector<redstone_torch_toggle> redstone_torch_recent_toggles;
    // Entities are never freed during a run either.
    // Sections are PersistentEntitySectionManager's, keyed by SectionPos.asLong of world positions, in signed order.
    std::vector<std::unique_ptr<entity>>     entity_store;
    std::vector<entity *>                    entity_tick_list;  // EntityTickList, in insertion order
    std::map<int64_t, std::vector<entity *>> entity_sections;
    int32_t entity_counter     = 0;   // Entity.ENTITY_COUNTER; the oracle resets it when the trace starts
    int32_t piston_noclip      = -1;  // PistonMovingBlockEntity.NOCLIP: the direction of the move pushing an entity
    // the force-loaded chunks, where entities tick
    int32_t loaded_chunk_min_x = 0;
    int32_t loaded_chunk_max_x = -1;
    int32_t loaded_chunk_min_z = 0;
    int32_t loaded_chunk_max_z = -1;

    trace *                            tr = nullptr;
    std::string                        unsupported;  // first unsupported mechanic hit; the run stops at the next check
    size_t                             unsupported_line = 0;  // trace lines before it, all still exact
    bool boundary = false;  // past an out-of-scope entity spawn nothing is contract, and the run ends
};

void level_unsupported(level & lv, const char * fmt, ...) __attribute__((format(printf, 2, 3)));
// traces the line and drops the level's trace: from here on nothing is traced
void level_boundary(level & lv, const char * fmt, ...) __attribute__((format(printf, 2, 3)));

state_id level_get_state(const level & lv, block_pos pos);

inline fluid_state_id level_get_fluid_state(const level & lv, block_pos pos) {
    return state_fluid_state(level_get_state(lv, pos));
}

struct level_section_ref {
    int64_t               key;  // SectionPos.asLong of the relative position
    const level_section * section;
};

// the sections with a block other than air, by key
std::vector<level_section_ref> level_sections(const level & lv);

// the section holding pos, null where nothing was ever set
const level_section * level_get_section(const level & lv, block_pos pos);

// Writes the state only: no updates, no block entity changes (case placement).
void level_set_state_raw(level & lv, block_pos pos, state_id st);

// Level.isInValidBounds, the same test as Level.isInWorldBounds
bool level_in_valid_bounds(block_pos pos);

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
// a write to the items of a block entity
void           block_entity_items_changed(level & lv, block_entity * be);

// Block.getDrops without a tool: the dumped drops of the state
std::vector<item_stack> block_get_drops(level & lv, state_id st, const block_entity * be);
// Block.dropResources without a tool
void                    level_drop_resources(level & lv, state_id st, block_pos pos, const block_entity * be);
// Block.popResource
void                    level_pop_resource(level & lv, block_pos pos, item_stack s);

void level_update_neighbors_at(level & lv, block_pos pos, int32_t block);
// updateNeighborsAt for each of the six neighbors, in Direction order
void level_update_neighbors_around(level & lv, block_pos pos, int32_t block);
void level_update_neighbors_at_except_from_facing(level & lv, block_pos pos, int32_t block, direction skip);
void level_neighbor_changed(level & lv, block_pos pos, int32_t block);
void level_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block, bool moved_by_piston);
void level_neighbor_shape_changed(level & lv, direction dir, block_pos pos, block_pos neighbor_pos, state_id neighbor,
                                  int32_t flags, int32_t limit);
void level_update_neighbour_for_output_signal(level & lv, block_pos pos, int32_t block);

void level_schedule_tick(level & lv, block_pos pos, int32_t block, int32_t delay,
                         int32_t priority = TICK_PRIORITY_NORMAL);
// Level.scheduleTick for a fluid, at normal priority
void level_schedule_fluid_tick(level & lv, block_pos pos, int32_t fluid, int32_t delay);
void level_block_event(level & lv, block_pos pos, int32_t block, int32_t param_a, int32_t param_b);

// SignalGetter
int32_t level_get_control_input_signal(level & lv, block_pos pos, direction dir, bool only_diodes);
int32_t level_get_direct_signal_to(level & lv, block_pos pos);
bool    level_has_signal(level & lv, block_pos pos, direction dir);
int32_t level_get_signal(level & lv, block_pos pos, direction dir);
bool    level_has_neighbor_signal(level & lv, block_pos pos);
int32_t level_get_best_neighbor_signal(level & lv, block_pos pos);

// ServerLevel.tick
void level_tick(level & lv);
