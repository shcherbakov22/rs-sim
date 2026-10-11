// Fluids: FlowingFluid with WaterFluid and LavaFluid runs the fluid ticks, LiquidBlock holds a fluid in its level property, and a SimpleWaterloggedBlock holds water. The overworld has no fast lava.
// Bubble columns and coral live in water.

#include "blocks/handlers.h"
#include "core/rng.h"
#include "engine/level.h"

#include <algorithm>

// Fluid

static bool fluid_is_lava(int32_t fluid) { return fluid == FLUID_LAVA || fluid == FLUID_FLOWING_LAVA; }

static bool fluid_is_water_tag(int32_t fluid) { return (fluid_defs[fluid].tags & FLUID_TAG_WATER) != 0; }

// WaterFluid.isSame and LavaFluid.isSame: a source and its flowing fluid are the same; fluid is never empty here
static bool fluid_is_same(int32_t fluid, int32_t other) {
    return other != FLUID_EMPTY && fluid_is_lava(fluid) == fluid_is_lava(other);
}

static int32_t fluid_get_flowing(int32_t fluid) {
    return fluid_is_lava(fluid) ? FLUID_FLOWING_LAVA : FLUID_FLOWING_WATER;
}

static int32_t fluid_get_source(int32_t fluid) { return fluid_is_lava(fluid) ? FLUID_LAVA : FLUID_WATER; }

int32_t fluid_get_tick_delay(int32_t fluid) { return fluid_is_lava(fluid) ? 30 : 5; }

static int32_t flowing_fluid_get_drop_off(int32_t fluid) { return fluid_is_lava(fluid) ? 2 : 1; }

static int32_t flowing_fluid_get_slope_find_distance(int32_t fluid) { return fluid_is_lava(fluid) ? 2 : 4; }

// the game rules waterSourceConversion (on) and lavaSourceConversion (off)
static bool flowing_fluid_can_convert_to_source(int32_t fluid) { return !fluid_is_lava(fluid); }

// FluidState

// FlowingFluid.getHeight: full under the same fluid, else amount / 9
float fluid_state_get_height(level & lv, fluid_state_id fs, block_pos pos) {
    int32_t above = fluid_state_get_type(level_get_fluid_state(lv, pos_relative(pos, DIRECTION_UP)));
    if (fluid_is_same(fluid_state_get_type(fs), above)) return 1.0f;
    return (float) fluid_state_get_amount(fs) / 9.0f;
}

// FlowingFluid.affectsFlow: the neighbor is empty or the same fluid
static bool flowing_fluid_affects_flow(int32_t fluid, fluid_state_id neighbor) {
    int32_t type = fluid_state_get_type(neighbor);
    return type == FLUID_EMPTY || fluid_is_same(fluid, type);
}

// FlowingFluid.isSolidFace: ice never is
static bool flowing_fluid_is_solid_face(level & lv, int32_t fluid, block_pos pos, direction dir) {
    if (fluid_is_same(fluid, fluid_state_get_type(level_get_fluid_state(lv, pos)))) return false;
    if (dir == DIRECTION_UP) return true;
    state_id st = level_get_state(lv, pos);
    return !block_instanceof(state_block(st), VCLASS_ICE_BLOCK) &&
           block_is_face_sturdy(lv, st, pos, dir, SUPPORT_TYPE_FULL);
}

// FlowingFluid.getFlow: toward lower neighbors in float steps of own height; a falling fluid next to a wall pulls down
vec3 flowing_fluid_get_flow(level & lv, block_pos pos, fluid_state_id fs) {
    int32_t fluid  = fluid_state_get_type(fs);
    float   own    = (float) fluid_state_get_amount(fs) / 9.0f;
    double  flow_x = 0.0;
    double  flow_z = 0.0;
    for (direction dir : DIRECTION_HORIZONTAL) {
        block_pos      neighbor_pos = pos_relative(pos, dir);
        fluid_state_id neighbor     = level_get_fluid_state(lv, neighbor_pos);
        if (!flowing_fluid_affects_flow(fluid, neighbor)) continue;
        float neighbor_height = (float) fluid_state_get_amount(neighbor) / 9.0f;
        float distance        = 0.0f;
        if (neighbor_height == 0.0f) {
            if (!state_has(level_get_state(lv, neighbor_pos), STATE_FLAG_BLOCKS_MOTION)) {
                fluid_state_id below = level_get_fluid_state(lv, pos_relative(neighbor_pos, DIRECTION_DOWN));
                if (flowing_fluid_affects_flow(fluid, below)) {
                    neighbor_height = (float) fluid_state_get_amount(below) / 9.0f;
                    if (neighbor_height > 0.0f) distance = own - (neighbor_height - 0.8888889f);
                }
            }
        } else if (neighbor_height > 0.0f) {
            distance = own - neighbor_height;
        }
        if (distance != 0.0f) {
            flow_x += (float) DIRECTION_STEPS[dir].x * distance;
            flow_z += (float) DIRECTION_STEPS[dir].z * distance;
        }
    }
    vec3 flow = { flow_x, 0.0, flow_z };
    if (fluid_state_is_falling(fs)) {
        for (direction dir : DIRECTION_HORIZONTAL) {
            block_pos side = pos_relative(pos, dir);
            if (flowing_fluid_is_solid_face(lv, fluid, side, dir) ||
                flowing_fluid_is_solid_face(lv, fluid, pos_relative(side, DIRECTION_UP), dir)) {
                flow = vec3_add(vec3_normalize(flow), { 0.0, -6.0, 0.0 });
                break;
            }
        }
    }
    return vec3_normalize(flow);
}

// empty takes anything, water only another fluid from above, lava only water and only at 4/9 high or more
static bool fluid_state_can_be_replaced_with(level & lv, fluid_state_id fs, block_pos pos, int32_t other,
                                             direction dir) {
    int32_t fluid = fluid_state_get_type(fs);
    if (fluid == FLUID_EMPTY) return true;
    if (!fluid_is_lava(fluid)) return dir == DIRECTION_DOWN && !fluid_is_water_tag(other);
    return fluid_state_get_height(lv, fs, pos) >= 0.44444445f && fluid_is_water_tag(other);
}

// FlowingFluid

// vanilla compares collision shapes with the Shapes.block() and Shapes.empty() instances; a dynamic shape comes out
// of VoxelShape.optimize, which returns those instances for a full and an empty shape
enum fluid_wall {
    FLUID_WALL_BLOCK,
    FLUID_WALL_EMPTY,
    FLUID_WALL_OTHER,
};

static fluid_wall fluid_wall_of(level & lv, state_id st, block_pos pos) {
    if (state_collision_shapes[st] != SHAPE_NONE) {
        if (state_has(st, STATE_FLAG_COLLISION_BLOCK)) return FLUID_WALL_BLOCK;
        return state_has(st, STATE_FLAG_COLLISION_EMPTY) ? FLUID_WALL_EMPTY : FLUID_WALL_OTHER;
    }
    shape s = block_get_collision_shape(lv, st, pos);
    if (shape_is_block(s)) return FLUID_WALL_BLOCK;
    return s.boxes.empty() ? FLUID_WALL_EMPTY : FLUID_WALL_OTHER;
}

// the occlusion cache only remembers answers
static bool flowing_fluid_can_pass_through_wall(level & lv, direction dir, block_pos source_pos, state_id source,
                                                block_pos target_pos, state_id target) {
    fluid_wall target_wall = fluid_wall_of(lv, target, target_pos);
    if (target_wall == FLUID_WALL_BLOCK) return false;
    fluid_wall source_wall = fluid_wall_of(lv, source, source_pos);
    if (source_wall == FLUID_WALL_BLOCK) return false;
    if (source_wall == FLUID_WALL_EMPTY && target_wall == FLUID_WALL_EMPTY) return true;
    return !shape_merged_face_occludes(block_get_collision_shape(lv, source, source_pos),
                                       block_get_collision_shape(lv, target, target_pos), dir);
}

static bool flowing_fluid_can_hold_any_fluid(state_id st) { return state_has(st, STATE_FLAG_FLUID_HOLDS_ANY); }

// a LiquidBlockContainer takes only a water source, and only if it has room
static bool flowing_fluid_can_hold_specific_fluid(state_id st, int32_t fluid) {
    return !state_has(st, STATE_FLAG_FLUID_CONTAINER) ||
           (fluid == FLUID_WATER && state_has(st, STATE_FLAG_FLUID_PLACE_WATER));
}

static bool flowing_fluid_can_hold_fluid(state_id st, int32_t fluid) {
    return flowing_fluid_can_hold_any_fluid(st) && flowing_fluid_can_hold_specific_fluid(st, fluid);
}

static bool flowing_fluid_is_source_block_of_this_type(int32_t fluid, fluid_state_id fs) {
    return fluid_is_same(fluid, fluid_state_get_type(fs)) && fluid_state_is_source(fs);
}

static bool flowing_fluid_can_maybe_pass_through(level & lv, int32_t fluid, block_pos source_pos, state_id source,
                                                 direction dir, block_pos test_pos, state_id test) {
    return !flowing_fluid_is_source_block_of_this_type(fluid, state_fluid_state(test)) &&
           flowing_fluid_can_hold_any_fluid(test) &&
           flowing_fluid_can_pass_through_wall(lv, dir, source_pos, source, test_pos, test);
}

static bool flowing_fluid_can_pass_through(level & lv, int32_t fluid, block_pos source_pos, state_id source,
                                           direction dir, block_pos test_pos, state_id test) {
    return flowing_fluid_can_maybe_pass_through(lv, fluid, source_pos, source, dir, test_pos, test) &&
           flowing_fluid_can_hold_specific_fluid(test, fluid_get_flowing(fluid));
}

static bool flowing_fluid_is_water_hole(level & lv, int32_t fluid, block_pos top_pos, state_id top,
                                        block_pos bottom_pos, state_id bottom) {
    if (!flowing_fluid_can_pass_through_wall(lv, DIRECTION_DOWN, top_pos, top, bottom_pos, bottom)) return false;
    return fluid_is_same(fluid, fluid_state_get_type(state_fluid_state(bottom))) ||
           flowing_fluid_can_hold_fluid(bottom, fluid_get_flowing(fluid));
}

// SpreadContext.isHole; its caches only remember reads of an unchanged world
static bool flowing_fluid_is_hole(level & lv, int32_t fluid, block_pos pos) {
    block_pos below = pos_relative(pos, DIRECTION_DOWN);
    return flowing_fluid_is_water_hole(lv, fluid, pos, level_get_state(lv, pos), below, level_get_state(lv, below));
}

static fluid_state_id flowing_fluid_get_new_liquid(level & lv, int32_t fluid, block_pos pos, state_id st) {
    int32_t highest_neighbor  = 0;
    int32_t neighbour_sources = 0;
    for (direction dir : DIRECTION_HORIZONTAL) {
        block_pos      rel = pos_relative(pos, dir);
        state_id       bs  = level_get_state(lv, rel);
        fluid_state_id fs  = state_fluid_state(bs);
        if (fluid_is_same(fluid, fluid_state_get_type(fs)) &&
            flowing_fluid_can_pass_through_wall(lv, dir, pos, st, rel, bs)) {
            if (fluid_state_is_source(fs)) neighbour_sources++;
            highest_neighbor = std::max(highest_neighbor, fluid_state_get_amount(fs));
        }
    }
    if (neighbour_sources >= 2 && flowing_fluid_can_convert_to_source(fluid)) {
        state_id below = level_get_state(lv, pos_relative(pos, DIRECTION_DOWN));
        if (state_has(below, STATE_FLAG_SOLID) ||
            flowing_fluid_is_source_block_of_this_type(fluid, state_fluid_state(below))) {
            return fluid_state_find(fluid_get_source(fluid), 8, false);
        }
    }
    block_pos      above_pos = pos_relative(pos, DIRECTION_UP);
    state_id       above     = level_get_state(lv, above_pos);
    fluid_state_id above_fs  = state_fluid_state(above);
    if (fluid_is_same(fluid, fluid_state_get_type(above_fs)) &&
        flowing_fluid_can_pass_through_wall(lv, DIRECTION_UP, pos, st, above_pos, above)) {
        return fluid_state_find(fluid_get_flowing(fluid), 8, true);
    }
    int32_t amount = highest_neighbor - flowing_fluid_get_drop_off(fluid);
    return amount <= 0 ? FLUID_STATE_EMPTY : fluid_state_find(fluid_get_flowing(fluid), amount, false);
}

// water washes the block away with its drops; lava only fizzes
static void flowing_fluid_before_destroying_block(level & lv, int32_t fluid, block_pos pos, state_id st) {
    if (fluid_is_lava(fluid)) return;
    level_drop_resources(lv, st, pos,
                         state_has(st, STATE_FLAG_BLOCK_ENTITY) ? level_get_block_entity(lv, pos) : nullptr);
}

// LavaFluid.spreadTo: lava falling on water makes stone of a water block, and is gone either way
static void flowing_fluid_spread_to(level & lv, int32_t fluid, block_pos pos, state_id st, direction dir,
                                    fluid_state_id target) {
    if (fluid_is_lava(fluid) && dir == DIRECTION_DOWN &&
        fluid_is_water_tag(fluid_state_get_type(level_get_fluid_state(lv, pos)))) {
        if (block_instanceof(state_block(st), VCLASS_LIQUID_BLOCK)) {
            level_set_block(lv, pos, block_default(BLOCK_STONE), UPDATE_FLAG_ALL);
        }
        return;
    }
    if (state_has(st, STATE_FLAG_FLUID_CONTAINER)) {
        block_place_liquid(lv, pos, st, target);
        return;
    }
    if (!state_has(st, STATE_FLAG_AIR)) flowing_fluid_before_destroying_block(lv, fluid, pos, st);
    level_set_block(lv, pos, fluid_state_create_legacy_block(target), UPDATE_FLAG_ALL);
}

static int32_t flowing_fluid_get_slope_distance(level & lv, int32_t fluid, block_pos pos, int32_t pass, direction from,
                                                state_id st) {
    int32_t lowest = 1000;
    for (direction dir : DIRECTION_HORIZONTAL) {
        if (dir == from) continue;
        block_pos test_pos = pos_relative(pos, dir);
        state_id  test     = level_get_state(lv, test_pos);
        if (!flowing_fluid_can_pass_through(lv, fluid, pos, st, dir, test_pos, test)) continue;
        if (flowing_fluid_is_hole(lv, fluid, test_pos)) return pass;
        if (pass < flowing_fluid_get_slope_find_distance(fluid)) {
            lowest = std::min(
                lowest, flowing_fluid_get_slope_distance(lv, fluid, test_pos, pass + 1, direction_opposite(dir), test));
        }
    }
    return lowest;
}

static int32_t flowing_fluid_source_neighbor_count(level & lv, int32_t fluid, block_pos pos) {
    int32_t count = 0;
    for (direction dir : DIRECTION_HORIZONTAL) {
        if (flowing_fluid_is_source_block_of_this_type(fluid, level_get_fluid_state(lv, pos_relative(pos, dir)))) {
            count++;
        }
    }
    return count;
}

// the sides with the shortest way down and their new fluid, marked in has by Direction ordinal (an EnumMap)
static void flowing_fluid_get_spread(level & lv, int32_t fluid, block_pos pos, state_id st, fluid_state_id spread[6],
                                     bool has[6]) {
    int32_t lowest = 1000;
    for (direction dir : DIRECTION_HORIZONTAL) {
        block_pos test_pos = pos_relative(pos, dir);
        state_id  test     = level_get_state(lv, test_pos);
        if (!flowing_fluid_can_maybe_pass_through(lv, fluid, pos, st, dir, test_pos, test)) continue;
        fluid_state_id new_fluid = flowing_fluid_get_new_liquid(lv, fluid, test_pos, test);
        if (!flowing_fluid_can_hold_specific_fluid(test, fluid_state_get_type(new_fluid))) continue;
        int32_t distance = flowing_fluid_is_hole(lv, fluid, test_pos) ?
                               0 :
                               flowing_fluid_get_slope_distance(lv, fluid, test_pos, 1, direction_opposite(dir), test);
        if (distance < lowest) std::fill(has, has + 6, false);
        if (distance <= lowest) {
            if (fluid_state_can_be_replaced_with(lv, state_fluid_state(test), test_pos, fluid_state_get_type(new_fluid),
                                                 dir)) {
                spread[dir] = new_fluid;
                has[dir]    = true;
            }
            lowest = distance;
        }
    }
}

static void flowing_fluid_spread_to_sides(level & lv, int32_t fluid, block_pos pos, fluid_state_id fs, state_id st) {
    int32_t neighbor = fluid_state_get_amount(fs) - flowing_fluid_get_drop_off(fluid);
    if (fluid_state_is_falling(fs)) neighbor = 7;
    if (neighbor <= 0) return;
    fluid_state_id spread[6] = {};
    bool           has[6]    = {};
    flowing_fluid_get_spread(lv, fluid, pos, st, spread, has);
    for (int32_t dir = 0; dir < 6; dir++) {
        if (!has[dir]) continue;
        block_pos neighbor_pos = pos_relative(pos, (direction) dir);
        flowing_fluid_spread_to(lv, fluid, neighbor_pos, level_get_state(lv, neighbor_pos), (direction) dir,
                                spread[dir]);
    }
}

// down first; a fluid that can fall spreads sideways too only from among three or more sources
static void flowing_fluid_spread(level & lv, int32_t fluid, block_pos pos, state_id st, fluid_state_id fs) {
    if (fluid_state_is_empty(fs)) return;
    block_pos below_pos = pos_relative(pos, DIRECTION_DOWN);
    state_id  below     = level_get_state(lv, below_pos);
    if (flowing_fluid_can_maybe_pass_through(lv, fluid, pos, st, DIRECTION_DOWN, below_pos, below)) {
        fluid_state_id new_below = flowing_fluid_get_new_liquid(lv, fluid, below_pos, below);
        int32_t        type      = fluid_state_get_type(new_below);
        if (fluid_state_can_be_replaced_with(lv, state_fluid_state(below), below_pos, type, DIRECTION_DOWN) &&
            flowing_fluid_can_hold_specific_fluid(below, type)) {
            flowing_fluid_spread_to(lv, fluid, below_pos, below, DIRECTION_DOWN, new_below);
            if (flowing_fluid_source_neighbor_count(lv, fluid, pos) >= 3) {
                flowing_fluid_spread_to_sides(lv, fluid, pos, fs, st);
            }
            return;
        }
    }
    if (fluid_state_is_source(fs) || !flowing_fluid_is_water_hole(lv, fluid, pos, st, below_pos, below)) {
        flowing_fluid_spread_to_sides(lv, fluid, pos, fs, st);
    }
}

// LavaFluid.getSpreadDelay: lava that rises draws whether to wait four times as long
static int32_t flowing_fluid_get_spread_delay(level & lv, int32_t fluid, block_pos pos, fluid_state_id old_fs,
                                              fluid_state_id new_fs) {
    int32_t delay = fluid_get_tick_delay(fluid);
    if (fluid_is_lava(fluid) && !fluid_state_is_empty(old_fs) && !fluid_state_is_empty(new_fs) &&
        !fluid_state_is_falling(old_fs) && !fluid_state_is_falling(new_fs) &&
        fluid_state_get_height(lv, new_fs, pos) > fluid_state_get_height(lv, old_fs, pos) &&
        rng_next_int(lv.random, 4) != 0) {
        delay *= 4;
    }
    return delay;
}

// a flowing fluid first follows its neighbors, then spreads; the spread keeps the type that ticked
void flowing_fluid_tick(level & lv, block_pos pos, state_id st, fluid_state_id fs) {
    int32_t fluid = fluid_state_get_type(fs);
    if (!fluid_state_is_source(fs)) {
        fluid_state_id new_fs = flowing_fluid_get_new_liquid(lv, fluid, pos, level_get_state(lv, pos));
        int32_t        delay  = flowing_fluid_get_spread_delay(lv, fluid, pos, fs, new_fs);
        if (fluid_state_is_empty(new_fs)) {
            fs = new_fs;
            st = STATE_AIR;
            level_set_block(lv, pos, st, UPDATE_FLAG_ALL);
        } else if (new_fs != fs) {
            fs = new_fs;
            st = fluid_state_create_legacy_block(fs);
            level_set_block(lv, pos, st, UPDATE_FLAG_ALL);
            level_schedule_fluid_tick(lv, pos, fluid_state_get_type(fs), delay);
        }
    }
    flowing_fluid_spread(lv, fluid, pos, st, fs);
}

// LiquidBlock: water and lava as blocks; lava next to water hardens instead of flowing

static int32_t liquid_block_fluid(state_id st) {
    return fluid_state_get_type(state_fluid_state(block_default(state_block(st))));
}

// lava touching water turns to obsidian (a source) or cobblestone, and over soul soil next to blue ice to basalt
static bool liquid_block_should_spread_liquid(level & lv, block_pos pos, state_id st) {
    if (!fluid_is_lava(liquid_block_fluid(st))) return true;
    bool over_soul_soil = state_is(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)), BLOCK_SOUL_SOIL);
    // POSSIBLE_FLOW_DIRECTIONS, each seen from the opposite neighbor
    for (direction dir : { DIRECTION_UP, DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_WEST, DIRECTION_EAST }) {
        block_pos neighbor_pos = pos_relative(pos, dir);
        if (fluid_is_water_tag(fluid_state_get_type(level_get_fluid_state(lv, neighbor_pos)))) {
            int32_t block = fluid_state_is_source(level_get_fluid_state(lv, pos)) ? BLOCK_OBSIDIAN : BLOCK_COBBLESTONE;
            level_set_block(lv, pos, block_default(block), UPDATE_FLAG_ALL);
            return false;
        }
        if (over_soul_soil && state_is(level_get_state(lv, neighbor_pos), BLOCK_BLUE_ICE)) {
            level_set_block(lv, pos, block_default(BLOCK_BASALT), UPDATE_FLAG_ALL);
            return false;
        }
    }
    return true;
}

static void liquid_block_schedule_tick(level & lv, block_pos pos, state_id st) {
    level_schedule_fluid_tick(lv, pos, fluid_state_get_type(state_fluid_state(st)),
                              fluid_get_tick_delay(liquid_block_fluid(st)));
}

static void liquid_block_on_place(level & lv, state_id st, block_pos pos, state_id, bool) {
    if (liquid_block_should_spread_liquid(lv, pos, st)) liquid_block_schedule_tick(lv, pos, st);
}

static state_id liquid_block_update_shape(level & lv, state_id st, block_pos pos, direction, block_pos,
                                          state_id neighbor) {
    if (fluid_state_is_source(state_fluid_state(st)) || fluid_state_is_source(state_fluid_state(neighbor))) {
        liquid_block_schedule_tick(lv, pos, st);
    }
    return st;
}

static void liquid_block_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    if (liquid_block_should_spread_liquid(lv, pos, st)) liquid_block_schedule_tick(lv, pos, st);
}

// only a source fills a bucket
static item_stack liquid_block_pickup_block(level & lv, block_pos pos, state_id st) {
    if (state_get(st, PROP_LEVEL) != 0) return {};
    level_set_block(lv, pos, STATE_AIR, UPDATE_FLAG_ALL_IMMEDIATE);
    return { fluid_is_lava(liquid_block_fluid(st)) ? ITEM_LAVA_BUCKET : ITEM_WATER_BUCKET, 1, 0 };
}

// BubbleColumnBlock: soul sand or magma below turns a water source column into bubbles

static bool bubble_column_can_exist_in(state_id st) {
    fluid_state_id fs = state_fluid_state(st);
    return state_is(st, BLOCK_BUBBLE_COLUMN) ||
           (state_is(st, BLOCK_WATER) && fluid_state_get_amount(fs) >= 8 && fluid_state_is_source(fs));
}

static state_id bubble_column_get_column_state(state_id below) {
    if (state_is(below, BLOCK_BUBBLE_COLUMN)) return below;
    if (state_is(below, BLOCK_SOUL_SAND)) return state_set(block_default(BLOCK_BUBBLE_COLUMN), PROP_DRAG, false);
    if (state_is(below, BLOCK_MAGMA_BLOCK)) return state_set(block_default(BLOCK_BUBBLE_COLUMN), PROP_DRAG, true);
    return block_default(BLOCK_WATER);
}

// the column goes up through water sources and bubbles; a set that changes nothing ends it
void bubble_column_update_column(level & lv, block_pos origin, state_id origin_state, state_id below) {
    if (!bubble_column_can_exist_in(origin_state)) return;
    state_id column = bubble_column_get_column_state(below);
    level_set_block(lv, origin, column, UPDATE_FLAG_CLIENTS);
    for (block_pos pos = pos_relative(origin, DIRECTION_UP); bubble_column_can_exist_in(level_get_state(lv, pos));
         pos           = pos_relative(pos, DIRECTION_UP)) {
        if (!level_set_block(lv, pos, column, UPDATE_FLAG_CLIENTS)) return;
    }
}

static void bubble_column_tick(level & lv, state_id st, block_pos pos) {
    bubble_column_update_column(lv, pos, st, level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)));
}

// Entity.onAboveBubbleColumn and onInsideBubbleColumn; above, the splash and bubble particles draw from the level random
static void bubble_column_entity_inside(level & lv, state_id st, block_pos pos, entity & e, entity_effect_collector &,
                                        bool is_precise) {
    if (!is_precise) return;
    state_id above = level_get_state(lv, pos_relative(pos, DIRECTION_UP));
    bool     drag  = state_get(st, PROP_DRAG);
    // vanilla asks for the shape of the state above at the column's own position
    if (block_get_collision_shape(lv, above, pos).boxes.empty() && fluid_state_is_empty(state_fluid_state(above))) {
        e.delta.y = drag ? std::max(-0.9, e.delta.y - 0.03) : std::min(1.8, e.delta.y + 0.1);
        for (int32_t i = 0; i < 8; i++) rng_next_double(lv.random);
    } else {
        e.delta.y       = drag ? std::max(-0.3, e.delta.y - 0.03) : std::min(0.7, e.delta.y + 0.06);
        e.fall_distance = 0.0;
    }
}

static bool bubble_column_can_survive(level & lv, state_id, block_pos pos) {
    state_id below = level_get_state(lv, pos_relative(pos, DIRECTION_DOWN));
    return state_is(below, BLOCK_BUBBLE_COLUMN) || state_is(below, BLOCK_MAGMA_BLOCK) ||
           state_is(below, BLOCK_SOUL_SAND);
}

static state_id bubble_column_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos,
                                           state_id neighbor) {
    level_schedule_fluid_tick(lv, pos, FLUID_WATER, fluid_get_tick_delay(FLUID_WATER));
    if (!block_can_survive(lv, st, pos) || dir == DIRECTION_DOWN ||
        (dir == DIRECTION_UP && !state_is(neighbor, BLOCK_BUBBLE_COLUMN) && bubble_column_can_exist_in(neighbor))) {
        level_schedule_tick(lv, pos, state_block(st), 5);
    }
    return st;
}

static item_stack bubble_column_pickup_block(level & lv, block_pos pos, state_id) {
    level_set_block(lv, pos, STATE_AIR, UPDATE_FLAG_ALL_IMMEDIATE);
    return { ITEM_WATER_BUCKET, 1, 0 };
}

// Coral: living coral dies 60 to 99 ticks after it finds no water in it or beside it

// CoralBlock.scanForWater
static bool coral_scan_for_water(level & lv, block_pos pos) {
    for (int32_t dir = DIRECTION_DOWN; dir <= DIRECTION_EAST; dir++) {
        if (fluid_is_water_tag(fluid_state_get_type(level_get_fluid_state(lv, pos_relative(pos, (direction) dir))))) {
            return true;
        }
    }
    return false;
}

static void coral_tick(level & lv, state_id st, block_pos pos) {
    if (coral_scan_for_water(lv, pos)) return;
    int32_t dead = block_field_get(state_block(st), FIELD_CORAL_BLOCK_DEAD_BLOCK);
    level_set_block(lv, pos, block_default(dead), UPDATE_FLAG_CLIENTS);
}

static state_id coral_update_shape(level & lv, state_id st, block_pos pos, direction, block_pos, state_id) {
    if (!coral_scan_for_water(lv, pos)) level_schedule_tick(lv, pos, state_block(st), 60 + rng_next_int(lv.random, 40));
    return st;
}

static bool base_coral_plant_type_scan_for_water(level & lv, state_id st, block_pos pos) {
    return state_get(st, PROP_WATERLOGGED) || coral_scan_for_water(lv, pos);
}

static void base_coral_plant_type_try_schedule_die_tick(level & lv, state_id st, block_pos pos) {
    if (!base_coral_plant_type_scan_for_water(lv, st, pos)) {
        level_schedule_tick(lv, pos, state_block(st), 60 + rng_next_int(lv.random, 40));
    }
}

static bool base_coral_plant_type_can_survive(level & lv, state_id, block_pos pos) {
    block_pos below = pos_relative(pos, DIRECTION_DOWN);
    return block_is_face_sturdy(lv, level_get_state(lv, below), below, DIRECTION_UP, SUPPORT_TYPE_FULL);
}

// block_update_shape schedules the water tick first
static state_id base_coral_plant_type_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos,
                                                   state_id) {
    return dir == DIRECTION_DOWN && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

static bool base_coral_wall_fan_can_survive(level & lv, state_id st, block_pos pos) {
    direction facing   = state_facing(st);
    block_pos attached = pos_relative(pos, direction_opposite(facing));
    return block_is_face_sturdy(lv, level_get_state(lv, attached), attached, facing, SUPPORT_TYPE_FULL);
}

// block_update_shape schedules the water tick first
static state_id base_coral_wall_fan_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos,
                                                 state_id) {
    return direction_opposite(dir) == state_facing(st) && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

// CoralPlantBlock, CoralFanBlock and CoralWallFanBlock
static void coral_plant_on_place(level & lv, state_id st, block_pos pos, state_id, bool) {
    base_coral_plant_type_try_schedule_die_tick(lv, st, pos);
}

// each living coral class keeps its own deadBlock field
static int32_t coral_plant_dead_block(int32_t block) {
    if (block_instanceof(block, VCLASS_CORAL_WALL_FAN_BLOCK)) {
        return block_field_get(block, FIELD_CORAL_WALL_FAN_BLOCK_DEAD_BLOCK);
    }
    if (block_instanceof(block, VCLASS_CORAL_FAN_BLOCK))
        return block_field_get(block, FIELD_CORAL_FAN_BLOCK_DEAD_BLOCK);
    return block_field_get(block, FIELD_CORAL_PLANT_BLOCK_DEAD_BLOCK);
}

// CoralPlantBlock, CoralFanBlock and CoralWallFanBlock; the wall fan keeps its facing
static void coral_plant_tick(level & lv, state_id st, block_pos pos) {
    if (base_coral_plant_type_scan_for_water(lv, st, pos)) return;
    state_id dead = block_with_properties_of(coral_plant_dead_block(state_block(st)), st);
    level_set_block(lv, pos, state_set(dead, PROP_WATERLOGGED, false), UPDATE_FLAG_CLIENTS);
}

// CoralPlantBlock and CoralFanBlock check survival first; the BaseCoralPlantTypeBlock updateShape they end in schedules the water tick again
static state_id coral_plant_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos neighbor_pos,
                                         state_id neighbor) {
    if (dir == DIRECTION_DOWN && !block_can_survive(lv, st, pos)) return STATE_AIR;
    base_coral_plant_type_try_schedule_die_tick(lv, st, pos);
    if (state_get(st, PROP_WATERLOGGED)) {
        level_schedule_fluid_tick(lv, pos, FLUID_WATER, fluid_get_tick_delay(FLUID_WATER));
        level_schedule_fluid_tick(lv, pos, FLUID_WATER, fluid_get_tick_delay(FLUID_WATER));
    }
    return base_coral_plant_type_update_shape(lv, st, pos, dir, neighbor_pos, neighbor);
}

// checks survival, schedules the water tick, then the die tick; the BaseCoralWallFanBlock updateShape it ends in schedules the water tick again
static state_id coral_wall_fan_update_shape(level & lv, state_id st, block_pos pos, direction dir,
                                            block_pos neighbor_pos, state_id neighbor) {
    if (direction_opposite(dir) == state_facing(st) && !block_can_survive(lv, st, pos)) return STATE_AIR;
    bool waterlogged = state_get(st, PROP_WATERLOGGED);
    if (waterlogged) level_schedule_fluid_tick(lv, pos, FLUID_WATER, fluid_get_tick_delay(FLUID_WATER));
    base_coral_plant_type_try_schedule_die_tick(lv, st, pos);
    if (waterlogged) level_schedule_fluid_tick(lv, pos, FLUID_WATER, fluid_get_tick_delay(FLUID_WATER));
    return base_coral_wall_fan_update_shape(lv, st, pos, dir, neighbor_pos, neighbor);
}

// clang-format off
const std::vector<block_hooks> fluid_hooks = {
    { .vclass = VCLASS_LIQUID_BLOCK,
      .update_shape = liquid_block_update_shape,
      .neighbor_changed = liquid_block_neighbor_changed,
      .on_place = liquid_block_on_place,
      .pickup_block = liquid_block_pickup_block },
    { .vclass = VCLASS_BUBBLE_COLUMN_BLOCK,
      .update_shape = bubble_column_update_shape,
      .tick = bubble_column_tick,
      .can_survive = bubble_column_can_survive,
      .pickup_block = bubble_column_pickup_block,
      .entity_inside = bubble_column_entity_inside },
    { .vclass = VCLASS_CORAL_BLOCK, .update_shape = coral_update_shape, .tick = coral_tick },
    { .vclass = VCLASS_BASE_CORAL_PLANT_TYPE_BLOCK,
      .update_shape = base_coral_plant_type_update_shape,
      .can_survive = base_coral_plant_type_can_survive },
    { .vclass = VCLASS_BASE_CORAL_WALL_FAN_BLOCK,
      .update_shape = base_coral_wall_fan_update_shape,
      .can_survive = base_coral_wall_fan_can_survive },
    { .vclass = VCLASS_CORAL_PLANT_BLOCK,
      .update_shape = coral_plant_update_shape,
      .on_place = coral_plant_on_place,
      .tick = coral_plant_tick },
    { .vclass = VCLASS_CORAL_FAN_BLOCK,
      .update_shape = coral_plant_update_shape,
      .on_place = coral_plant_on_place,
      .tick = coral_plant_tick },
    { .vclass = VCLASS_CORAL_WALL_FAN_BLOCK,
      .update_shape = coral_wall_fan_update_shape,
      .on_place = coral_plant_on_place,
      .tick = coral_plant_tick },
};
// clang-format on
