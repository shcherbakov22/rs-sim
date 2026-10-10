// Terrain: falling blocks, scaffolding, leaves, tnt, soul sand and dirt that react to the blocks around them

#include "blocks/handlers.h"
#include "core/rng.h"
#include "core/trace.h"
#include "engine/level.h"

#include <algorithm>

// FallingBlockEntity.fall clears the block to its fluid before the spawn, which ends the run
static void falling_block_entity_fall(level & lv, state_id st, block_pos pos) {
    if (state_has_prop(st, PROP_WATERLOGGED) && state_get(st, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
        return;
    }
    level_set_block(lv, pos, STATE_AIR, UPDATE_FLAG_ALL);
    trace_boundary(lv.tr, "spawn falling_block %d %d %d", pos.x, pos.y, pos.z);
}

// FallingBlock: sand, gravel, concrete powder and the like fall when the block below is free

// getDelayAfterPlace
static const int32_t FALLING_BLOCK_DELAY_AFTER_PLACE = 2;

static void falling_block_on_place(level & lv, state_id st, block_pos pos, state_id, bool) {
    level_schedule_tick(lv, pos, state_block(st), FALLING_BLOCK_DELAY_AFTER_PLACE);
}

static state_id falling_block_update_shape(level & lv, state_id st, block_pos pos, direction, block_pos, state_id) {
    level_schedule_tick(lv, pos, state_block(st), FALLING_BLOCK_DELAY_AFTER_PLACE);
    return st;
}

static void falling_block_tick(level & lv, state_id st, block_pos pos) {
    if (state_has(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)), STATE_FLAG_FALLING_FREE) &&
        pos.y >= LEVEL_Y_MIN) {
        falling_block_entity_fall(lv, st, pos);
    }
}

// ConcretePowderBlock: becomes concrete where it touches water. The landing of a falling powder is an entity matter.

// the face test runs on the neighbor's state but with the powder's position, as vanilla passes it
static bool concrete_powder_touches_liquid(level & lv, block_pos pos) {
    for (int32_t dir = DIRECTION_DOWN; dir <= DIRECTION_EAST; dir++) {
        if (dir == DIRECTION_DOWN && !state_has(level_get_state(lv, pos), STATE_FLAG_WATER)) continue;
        state_id neighbor = level_get_state(lv, pos_relative(pos, (direction) dir));
        if (state_has(neighbor, STATE_FLAG_WATER) &&
            !block_is_face_sturdy(lv, neighbor, pos, direction_opposite((direction) dir), SUPPORT_TYPE_FULL)) {
            return true;
        }
    }
    return false;
}

static state_id concrete_powder_update_shape(level &   lv,
                                             state_id  st,
                                             block_pos pos,
                                             direction dir,
                                             block_pos neighbor_pos,
                                             state_id  neighbor) {
    if (concrete_powder_touches_liquid(lv, pos)) {
        return block_default(block_field_get(state_block(st), FIELD_CONCRETE_POWDER_BLOCK_CONCRETE));
    }
    return falling_block_update_shape(lv, st, pos, dir, neighbor_pos, neighbor);
}

// ScaffoldingBlock: distance 0 on a sturdy block, else one more than the nearest scaffolding beside it; 7 falls

static int32_t scaffolding_get_distance(level & lv, block_pos pos) {
    block_pos below       = pos_relative(pos, DIRECTION_DOWN);
    state_id  below_state = level_get_state(lv, below);
    int32_t   distance    = 7;
    if (state_is(below_state, BLOCK_SCAFFOLDING)) {
        distance = state_get(below_state, PROP_STABILITY_DISTANCE);
    } else if (block_is_face_sturdy(lv, below_state, below, DIRECTION_UP, SUPPORT_TYPE_FULL)) {
        return 0;
    }
    for (direction dir : DIRECTION_HORIZONTAL) {
        state_id neighbor = level_get_state(lv, pos_relative(pos, dir));
        if (state_is(neighbor, BLOCK_SCAFFOLDING)) {
            distance = std::min(distance, state_get(neighbor, PROP_STABILITY_DISTANCE) + 1);
            if (distance == 1) break;
        }
    }
    return distance;
}

static bool scaffolding_is_bottom(level & lv, state_id st, block_pos pos, int32_t distance) {
    return distance > 0 && !state_is(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)), state_block(st));
}

static void scaffolding_on_place(level & lv, state_id st, block_pos pos, state_id, bool) {
    level_schedule_tick(lv, pos, state_block(st), 1);
}

static state_id scaffolding_update_shape(level & lv, state_id st, block_pos pos, direction, block_pos, state_id) {
    level_schedule_tick(lv, pos, state_block(st), 1);
    return st;
}

static void scaffolding_tick(level & lv, state_id st, block_pos pos) {
    int32_t  distance = scaffolding_get_distance(lv, pos);
    state_id next     = state_set(state_set(st, PROP_STABILITY_DISTANCE, distance), PROP_BOTTOM,
                                  scaffolding_is_bottom(lv, st, pos, distance));
    if (distance == 7) {
        if (state_get(st, PROP_STABILITY_DISTANCE) == 7) {
            falling_block_entity_fall(lv, st, pos);
        } else {
            level_destroy_block(lv, pos, true);
        }
    } else if (st != next) {
        level_set_block(lv, pos, next, UPDATE_FLAG_ALL);
    }
}

// LeavesBlock: distance to the nearest log, refreshed one tick after a neighbor changes

// getOptionalDistanceAt, 7 when the state has no distance
static int32_t leaves_get_distance_at(state_id st) {
    if (state_has(st, STATE_FLAG_TAG_LOGS)) return 0;
    return state_has_prop(st, PROP_DISTANCE) ? state_get(st, PROP_DISTANCE) : 7;
}

// updateDistance
static void leaves_tick(level & lv, state_id st, block_pos pos) {
    int32_t distance = 7;
    for (int32_t d = 0; d < 6 && distance > 1; d++) {
        distance =
            std::min(distance, leaves_get_distance_at(level_get_state(lv, pos_relative(pos, (direction) d))) + 1);
    }
    level_set_block(lv, pos, state_set(st, PROP_DISTANCE, distance), UPDATE_FLAG_ALL);
}

// only a log beside leaves at distance 1 skips the tick
static state_id leaves_update_shape(level & lv, state_id st, block_pos pos, direction, block_pos, state_id neighbor) {
    int32_t distance_from_neighbor = leaves_get_distance_at(neighbor) + 1;
    if (distance_from_neighbor != 1 || state_get(st, PROP_DISTANCE) != distance_from_neighbor) {
        level_schedule_tick(lv, pos, state_block(st), 1);
    }
    return st;
}

// TntBlock. Cases keep the game rule TNT_EXPLODES on, so priming always succeeds.

// the PrimedTnt constructor draws its push angle from the level random before the spawn
void tnt_prime(level & lv, block_pos pos) {
    rng_next_double(lv.random);
    trace_boundary(lv.tr, "spawn tnt %d %d %d", pos.x, pos.y, pos.z);
}

// the spawn ends the run, so the removal of the block after priming is left out
static void tnt_on_place(level & lv, state_id st, block_pos pos, state_id old, bool) {
    if (!state_is(old, state_block(st)) && level_has_neighbor_signal(lv, pos)) tnt_prime(lv, pos);
}

static void tnt_neighbor_changed(level & lv, state_id, block_pos pos, int32_t, bool) {
    if (level_has_neighbor_signal(lv, pos)) tnt_prime(lv, pos);
}

// SoulSandBlock: keeps a bubble column above it, which only water can hold

static void soul_sand_on_place(level & lv, state_id st, block_pos pos, state_id, bool) {
    level_schedule_tick(lv, pos, state_block(st), 20);
}

static state_id soul_sand_update_shape(level &   lv,
                                       state_id  st,
                                       block_pos pos,
                                       direction dir,
                                       block_pos,
                                       state_id neighbor) {
    if (dir == DIRECTION_UP && state_is(neighbor, BLOCK_WATER)) level_schedule_tick(lv, pos, state_block(st), 20);
    return st;
}

// BubbleColumnBlock.updateColumn: a bubble column or a water source above becomes a column
static void soul_sand_tick(level & lv, state_id, block_pos pos) {
    state_id above = level_get_state(lv, pos_relative(pos, DIRECTION_UP));
    if (state_is(above, BLOCK_BUBBLE_COLUMN) || (state_is(above, BLOCK_WATER) && state_get(above, PROP_LEVEL) == 0)) {
        level_unsupported(lv, "fluids");
    }
}

// SnowyDirtBlock: grass, podzol and mycelium show snow while a block tagged snow is on top
static state_id snowy_dirt_update_shape(level &, state_id st, block_pos, direction dir, block_pos, state_id neighbor) {
    return dir == DIRECTION_UP ? state_set(st, PROP_SNOWY, state_has(neighbor, STATE_FLAG_TAG_SNOW)) : st;
}

// DirtPathBlock: turns back into dirt a tick after a solid block (other than a fence gate) lands on top

static bool dirt_path_can_survive(level & lv, state_id, block_pos pos) {
    state_id above = level_get_state(lv, pos_relative(pos, DIRECTION_UP));
    return !state_has(above, STATE_FLAG_SOLID) || block_instanceof(state_block(above), VCLASS_FENCE_GATE_BLOCK);
}

static state_id dirt_path_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos, state_id) {
    if (dir == DIRECTION_UP && !block_can_survive(lv, st, pos)) level_schedule_tick(lv, pos, state_block(st), 1);
    return st;
}

// FarmBlock.turnToDirt
static void dirt_path_tick(level & lv, state_id, block_pos pos) {
    level_set_block(lv, pos, block_default(BLOCK_DIRT), UPDATE_FLAG_ALL);
}

// clang-format off
const std::vector<block_hooks> terrain_hooks = {
    { .vclass = VCLASS_FALLING_BLOCK,
      .update_shape = falling_block_update_shape,
      .on_place = falling_block_on_place,
      .tick = falling_block_tick },
    { .vclass = VCLASS_CONCRETE_POWDER_BLOCK, .update_shape = concrete_powder_update_shape },
    { .vclass = VCLASS_SCAFFOLDING_BLOCK,
      .update_shape = scaffolding_update_shape,
      .on_place = scaffolding_on_place,
      .tick = scaffolding_tick },
    { .vclass = VCLASS_LEAVES_BLOCK, .update_shape = leaves_update_shape, .tick = leaves_tick },
    { .vclass = VCLASS_TNT_BLOCK, .neighbor_changed = tnt_neighbor_changed, .on_place = tnt_on_place },
    { .vclass = VCLASS_SOUL_SAND_BLOCK,
      .update_shape = soul_sand_update_shape,
      .on_place = soul_sand_on_place,
      .tick = soul_sand_tick },
    { .vclass = VCLASS_SNOWY_DIRT_BLOCK, .update_shape = snowy_dirt_update_shape },
    { .vclass = VCLASS_DIRT_PATH_BLOCK,
      .update_shape = dirt_path_update_shape,
      .tick = dirt_path_tick,
      .can_survive = dirt_path_can_survive },
};
// clang-format on
