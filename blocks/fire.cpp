// Fire: fire and soul fire burn on a support, and fire inside an empty obsidian frame lights a nether portal

#include "blocks/handlers.h"
#include "core/rng.h"
#include "engine/level.h"

#include <bit>

// PortalShape: an empty obsidian frame 2..21 wide and 3..21 high

constexpr int32_t PORTAL_SHAPE_MAX_SIZE = 21;

struct portal_shape {
    direction_axis axis;
    int32_t        num_portal_blocks;
    direction      right_dir;
    block_pos      bottom_left;
    int32_t        height;
    int32_t        width;
};

static bool portal_shape_is_frame(state_id st) { return state_is(st, BLOCK_OBSIDIAN); }

static bool portal_shape_is_empty(state_id st) {
    return state_has(st, STATE_FLAG_AIR) || state_has(st, STATE_FLAG_TAG_FIRE) || state_is(st, BLOCK_NETHER_PORTAL);
}

static block_pos portal_shape_at(block_pos bottom_left, direction right_dir, int32_t up, int32_t right) {
    return pos_relative_steps(pos_relative_steps(bottom_left, DIRECTION_UP, up), right_dir, right);
}

static int32_t portal_shape_get_distance_until_edge_above_frame(level & lv, block_pos pos, direction dir) {
    for (int32_t width = 0; width <= PORTAL_SHAPE_MAX_SIZE; width++) {
        block_pos at = pos_relative_steps(pos, dir, width);
        state_id  st = level_get_state(lv, at);
        if (!portal_shape_is_empty(st)) {
            if (portal_shape_is_frame(st)) return width;
            break;
        }
        if (!portal_shape_is_frame(level_get_state(lv, pos_relative(at, DIRECTION_DOWN)))) break;
    }
    return 0;
}

// false for vanilla's null
static bool portal_shape_calculate_bottom_left(level & lv, direction right_dir, block_pos pos, block_pos & out) {
    int32_t min_y = std::max(LEVEL_Y_MIN, pos.y - PORTAL_SHAPE_MAX_SIZE);
    while (pos.y > min_y && portal_shape_is_empty(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)))) {
        pos = pos_relative(pos, DIRECTION_DOWN);
    }
    direction left_dir = direction_opposite(right_dir);
    int32_t   edge     = portal_shape_get_distance_until_edge_above_frame(lv, pos, left_dir) - 1;
    if (edge < 0) return false;
    out = pos_relative_steps(pos, left_dir, edge);
    return true;
}

static int32_t portal_shape_calculate_width(level & lv, block_pos bottom_left, direction right_dir) {
    int32_t width = portal_shape_get_distance_until_edge_above_frame(lv, bottom_left, right_dir);
    return width >= 2 && width <= PORTAL_SHAPE_MAX_SIZE ? width : 0;
}

static int32_t portal_shape_get_distance_until_top(level & lv, block_pos bottom_left, direction right_dir,
                                                   int32_t width, int32_t & portal_block_count) {
    for (int32_t height = 0; height < PORTAL_SHAPE_MAX_SIZE; height++) {
        if (!portal_shape_is_frame(level_get_state(lv, portal_shape_at(bottom_left, right_dir, height, -1))) ||
            !portal_shape_is_frame(level_get_state(lv, portal_shape_at(bottom_left, right_dir, height, width)))) {
            return height;
        }
        for (int32_t i = 0; i < width; i++) {
            state_id st = level_get_state(lv, portal_shape_at(bottom_left, right_dir, height, i));
            if (!portal_shape_is_empty(st)) return height;
            if (state_is(st, BLOCK_NETHER_PORTAL)) portal_block_count++;
        }
    }
    return PORTAL_SHAPE_MAX_SIZE;
}

static bool portal_shape_has_top_frame(level & lv, block_pos bottom_left, direction right_dir, int32_t width,
                                       int32_t height) {
    for (int32_t i = 0; i < width; i++) {
        if (!portal_shape_is_frame(level_get_state(lv, portal_shape_at(bottom_left, right_dir, height, i)))) {
            return false;
        }
    }
    return true;
}

static int32_t portal_shape_calculate_height(level & lv, block_pos bottom_left, direction right_dir, int32_t width,
                                             int32_t & portal_block_count) {
    int32_t height = portal_shape_get_distance_until_top(lv, bottom_left, right_dir, width, portal_block_count);
    bool    valid  = height >= 3 && height <= PORTAL_SHAPE_MAX_SIZE &&
                     portal_shape_has_top_frame(lv, bottom_left, right_dir, width, height);
    return valid ? height : 0;
}

static portal_shape portal_shape_find_any_shape(level & lv, block_pos pos, direction_axis axis) {
    direction right_dir = axis == DIRECTION_AXIS_X ? DIRECTION_WEST : DIRECTION_SOUTH;
    block_pos bottom_left;
    if (!portal_shape_calculate_bottom_left(lv, right_dir, pos, bottom_left)) return { axis, 0, right_dir, pos, 0, 0 };
    int32_t width = portal_shape_calculate_width(lv, bottom_left, right_dir);
    if (width == 0) return { axis, 0, right_dir, bottom_left, 0, 0 };
    int32_t portal_block_count = 0;
    int32_t height             = portal_shape_calculate_height(lv, bottom_left, right_dir, width, portal_block_count);
    return { axis, portal_block_count, right_dir, bottom_left, height, width };
}

static bool portal_shape_is_valid(const portal_shape & s) {
    return s.width >= 2 && s.width <= PORTAL_SHAPE_MAX_SIZE && s.height >= 3 && s.height <= PORTAL_SHAPE_MAX_SIZE;
}

// PortalShape.findPortalShape with the predicate of findEmptyPortalShape; false for an empty Optional
static bool portal_shape_find_empty_portal_shape(level & lv, block_pos pos, direction_axis preferred_axis,
                                                 portal_shape & out) {
    direction_axis axes[2] = { preferred_axis,
                               preferred_axis == DIRECTION_AXIS_X ? DIRECTION_AXIS_Z : DIRECTION_AXIS_X };
    for (direction_axis axis : axes) {
        out = portal_shape_find_any_shape(lv, pos, axis);
        if (portal_shape_is_valid(out) && out.num_portal_blocks == 0) return true;
    }
    return false;
}

static bool portal_shape_is_complete(const portal_shape & s) {
    return portal_shape_is_valid(s) && s.num_portal_blocks == s.width * s.height;
}

// BlockPos.betweenClosed walks x fastest, then y, then z
static void portal_shape_create_portal_blocks(level & lv, const portal_shape & s) {
    state_id  st  = state_set(block_default(BLOCK_NETHER_PORTAL), PROP_HORIZONTAL_AXIS, s.axis);
    block_pos a   = s.bottom_left;
    block_pos b   = portal_shape_at(s.bottom_left, s.right_dir, s.height - 1, s.width - 1);
    block_pos min = { std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) };
    block_pos max = { std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) };
    for (int32_t z = min.z; z <= max.z; z++) {
        for (int32_t y = min.y; y <= max.y; y++) {
            for (int32_t x = min.x; x <= max.x; x++) {
                level_set_block(lv, { x, y, z }, st, UPDATE_FLAG_CLIENTS | UPDATE_FLAG_KNOWN_SHAPE);
            }
        }
    }
}

// BaseFireBlock.entityInside: ignite, then hurt by the block's fire damage
static void base_fire_entity_inside(level &, state_id st, block_pos, entity &, entity_effect_collector & effects,
                                    bool) {
    float damage = std::bit_cast<float>(block_field_get(state_block(st), FIELD_BASE_FIRE_BLOCK_FIRE_DAMAGE));
    entity_effect_collector_apply(effects, INSIDE_BLOCK_EFFECT_TYPE_CLEAR_FREEZE);
    entity_effect_collector_apply(effects, INSIDE_BLOCK_EFFECT_TYPE_FIRE_IGNITE);
    entity_effect_collector_run_after(effects, INSIDE_BLOCK_EFFECT_TYPE_FIRE_IGNITE,
                                      { ENTITY_EFFECT_HURT, DAMAGE_TYPE_IN_FIRE, damage });
}

// BaseFireBlock.onPlace: the oracle level is the overworld, a portal dimension
static void base_fire_on_place(level & lv, state_id st, block_pos pos, state_id old, bool) {
    if (state_is(old, state_block(st))) return;
    portal_shape shape;
    if (portal_shape_find_empty_portal_shape(lv, pos, DIRECTION_AXIS_X, shape)) {
        portal_shape_create_portal_blocks(lv, shape);
        return;
    }
    if (!block_can_survive(lv, st, pos)) level_remove_block(lv, pos, false);
}

// SoulFireBlock: fire that lives only on soul fire base blocks

static bool soul_fire_can_survive_on_block(state_id st) { return state_has(st, STATE_FLAG_TAG_SOUL_FIRE_BASE_BLOCKS); }

static bool soul_fire_can_survive(level & lv, state_id, block_pos pos) {
    return soul_fire_can_survive_on_block(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)));
}

static state_id soul_fire_update_shape(level & lv, state_id st, block_pos pos, direction, block_pos, state_id) {
    return soul_fire_can_survive(lv, st, pos) ? block_default(state_block(st)) : STATE_AIR;
}

// FireBlock: fire burns on a sturdy top face or next to a flammable block; its sides show where it climbs

// FireBlock.canBurn
static bool fire_can_burn(state_id st) { return state_has(st, STATE_FLAG_FIRE_CAN_BURN); }

// FireBlock.getStateForPlacement(BlockGetter, BlockPos)
static state_id fire_get_state_for_placement(level & lv, block_pos pos) {
    block_pos below_pos = pos_relative(pos, DIRECTION_DOWN);
    state_id  below     = level_get_state(lv, below_pos);
    state_id  result    = block_default(BLOCK_FIRE);
    if (fire_can_burn(below) || block_is_face_sturdy(lv, below, below_pos, DIRECTION_UP, SUPPORT_TYPE_FULL)) {
        return result;
    }
    for (int32_t d = DIRECTION_UP; d < 6; d++) {
        bool burn = fire_can_burn(level_get_state(lv, pos_relative(pos, (direction) d)));
        result    = state_set(result, FIRE_BLOCK_PROPERTY_BY_DIRECTION[d], burn);
    }
    return result;
}

static state_id fire_get_state_with_age(level & lv, block_pos pos, int32_t age) {
    state_id st = base_fire_get_state(lv, pos);
    return state_is(st, BLOCK_FIRE) ? state_set(st, PROP_AGE_15, age) : st;
}

static bool fire_is_valid_fire_location(level & lv, block_pos pos) {
    for (int32_t d = 0; d < 6; d++) {
        if (fire_can_burn(level_get_state(lv, pos_relative(pos, (direction) d)))) return true;
    }
    return false;
}

static bool fire_can_survive(level & lv, state_id, block_pos pos) {
    block_pos below_pos = pos_relative(pos, DIRECTION_DOWN);
    return block_is_face_sturdy(lv, level_get_state(lv, below_pos), below_pos, DIRECTION_UP, SUPPORT_TYPE_FULL) ||
           fire_is_valid_fire_location(lv, pos);
}

static state_id fire_update_shape(level & lv, state_id st, block_pos pos, direction, block_pos, state_id) {
    return fire_can_survive(lv, st, pos) ? fire_get_state_with_age(lv, pos, state_get(st, PROP_AGE_15)) : STATE_AIR;
}

static int32_t fire_get_fire_tick_delay(rng & random) { return 30 + rng_next_int(random, 10); }

// the tick is scheduled even when BaseFireBlock.onPlace removed the fire
static void fire_on_place(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston) {
    base_fire_on_place(lv, st, pos, old, moved_by_piston);
    level_schedule_tick(lv, pos, BLOCK_FIRE, fire_get_fire_tick_delay(lv.random));
}

// ServerLevel.canSpreadFireAround is false with no player near, and the oracle level has none: fire neither spreads nor burns out
static void fire_tick(level & lv, state_id, block_pos pos) {
    level_schedule_tick(lv, pos, BLOCK_FIRE, fire_get_fire_tick_delay(lv.random));
}

// BaseFireBlock.getState: soul fire on soul sand or soil, else plain fire
state_id base_fire_get_state(level & lv, block_pos pos) {
    state_id below = level_get_state(lv, pos_relative(pos, DIRECTION_DOWN));
    return soul_fire_can_survive_on_block(below) ? block_default(BLOCK_SOUL_FIRE) :
                                                   fire_get_state_for_placement(lv, pos);
}

static bool base_fire_is_portal(level & lv, block_pos pos, direction forward) {
    bool has_obsidian = false;
    for (int32_t d = 0; d < 6; d++) {
        has_obsidian |= state_is(level_get_state(lv, pos_relative(pos, (direction) d)), BLOCK_OBSIDIAN);
    }
    if (!has_obsidian) return false;
    direction_axis axis;
    if (direction_is_horizontal(forward)) {
        axis = direction_get_axis(direction_counter_clockwise(forward));
    } else {
        // Direction.Plane.HORIZONTAL.getRandomAxis: a dispenser facing up or down
        axis = rng_next_int(lv.random, 2) == 0 ? DIRECTION_AXIS_X : DIRECTION_AXIS_Z;
    }
    portal_shape shape;
    return portal_shape_find_empty_portal_shape(lv, pos, axis, shape);
}

bool base_fire_can_be_placed_at(level & lv, block_pos pos, direction forward) {
    if (!state_has(level_get_state(lv, pos), STATE_FLAG_AIR)) return false;
    return block_can_survive(lv, base_fire_get_state(lv, pos), pos) || base_fire_is_portal(lv, pos, forward);
}

// NetherPortalBlock: a portal block breaks when an update along its plane finds the frame no longer complete

// updates across the portal plane leave it alone
static state_id nether_portal_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos,
                                           state_id neighbor) {
    direction_axis update_axis = direction_get_axis(dir);
    direction_axis axis        = (direction_axis) state_get(st, PROP_HORIZONTAL_AXIS);
    bool           wrong_axis  = axis != update_axis && update_axis != DIRECTION_AXIS_Y;
    if (!wrong_axis && !state_is(neighbor, BLOCK_NETHER_PORTAL) &&
        !portal_shape_is_complete(portal_shape_find_any_shape(lv, pos, axis))) {
        return STATE_AIR;
    }
    return st;
}

// clang-format off
const std::vector<block_hooks> fire_hooks = {
    { .vclass = VCLASS_BASE_FIRE_BLOCK, .on_place = base_fire_on_place, .entity_inside = base_fire_entity_inside },
    { .vclass = VCLASS_FIRE_BLOCK,
      .update_shape = fire_update_shape,
      .on_place = fire_on_place,
      .tick = fire_tick,
      .can_survive = fire_can_survive },
    { .vclass = VCLASS_SOUL_FIRE_BLOCK, .update_shape = soul_fire_update_shape, .can_survive = soul_fire_can_survive },
    { .vclass = VCLASS_NETHER_PORTAL_BLOCK, .update_shape = nether_portal_update_shape },
};
// clang-format on
