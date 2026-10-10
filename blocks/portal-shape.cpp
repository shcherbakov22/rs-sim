// PortalShape: an empty obsidian frame 2..21 wide and 3..21 high that fire turns into a nether portal

#include "blocks/handlers.h"
#include "engine/level.h"

constexpr int32_t PORTAL_SHAPE_MAX_SIZE = 21;

static bool portal_shape_is_frame(state_id st) {
    return state_is(st, BLOCK_OBSIDIAN);
}

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
            if (portal_shape_is_frame(st)) {
                return width;
            }
            break;
        }
        if (!portal_shape_is_frame(level_get_state(lv, pos_relative(at, DIRECTION_DOWN)))) {
            break;
        }
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
    if (edge < 0) {
        return false;
    }
    out = pos_relative_steps(pos, left_dir, edge);
    return true;
}

static int32_t portal_shape_calculate_width(level & lv, block_pos bottom_left, direction right_dir) {
    int32_t width = portal_shape_get_distance_until_edge_above_frame(lv, bottom_left, right_dir);
    return width >= 2 && width <= PORTAL_SHAPE_MAX_SIZE ? width : 0;
}

static int32_t portal_shape_get_distance_until_top(level &   lv,
                                                   block_pos bottom_left,
                                                   direction right_dir,
                                                   int32_t   width,
                                                   int32_t & portal_block_count) {
    for (int32_t height = 0; height < PORTAL_SHAPE_MAX_SIZE; height++) {
        if (!portal_shape_is_frame(level_get_state(lv, portal_shape_at(bottom_left, right_dir, height, -1)))) {
            return height;
        }
        if (!portal_shape_is_frame(level_get_state(lv, portal_shape_at(bottom_left, right_dir, height, width)))) {
            return height;
        }
        for (int32_t i = 0; i < width; i++) {
            state_id st = level_get_state(lv, portal_shape_at(bottom_left, right_dir, height, i));
            if (!portal_shape_is_empty(st)) {
                return height;
            }
            if (state_is(st, BLOCK_NETHER_PORTAL)) {
                portal_block_count++;
            }
        }
    }
    return PORTAL_SHAPE_MAX_SIZE;
}

static bool portal_shape_has_top_frame(level &   lv,
                                       block_pos bottom_left,
                                       direction right_dir,
                                       int32_t   width,
                                       int32_t   height) {
    for (int32_t i = 0; i < width; i++) {
        if (!portal_shape_is_frame(level_get_state(lv, portal_shape_at(bottom_left, right_dir, height, i)))) {
            return false;
        }
    }
    return true;
}

static int32_t portal_shape_calculate_height(level &   lv,
                                             block_pos bottom_left,
                                             direction right_dir,
                                             int32_t   width,
                                             int32_t & portal_block_count) {
    int32_t height = portal_shape_get_distance_until_top(lv, bottom_left, right_dir, width, portal_block_count);
    bool    valid  = height >= 3 && height <= PORTAL_SHAPE_MAX_SIZE &&
                     portal_shape_has_top_frame(lv, bottom_left, right_dir, width, height);
    return valid ? height : 0;
}

portal_shape portal_shape_find_any_shape(level & lv, block_pos pos, direction_axis axis) {
    direction right_dir = axis == DIRECTION_AXIS_X ? DIRECTION_WEST : DIRECTION_SOUTH;
    block_pos bottom_left;
    if (!portal_shape_calculate_bottom_left(lv, right_dir, pos, bottom_left)) {
        return { axis, 0, right_dir, pos, 0, 0 };
    }
    int32_t width = portal_shape_calculate_width(lv, bottom_left, right_dir);
    if (width == 0) {
        return { axis, 0, right_dir, bottom_left, 0, 0 };
    }
    int32_t portal_block_count = 0;
    int32_t height             = portal_shape_calculate_height(lv, bottom_left, right_dir, width, portal_block_count);
    return { axis, portal_block_count, right_dir, bottom_left, height, width };
}

static bool portal_shape_is_valid(const portal_shape & s) {
    return s.width >= 2 && s.width <= PORTAL_SHAPE_MAX_SIZE && s.height >= 3 && s.height <= PORTAL_SHAPE_MAX_SIZE;
}

// PortalShape.findPortalShape with the predicate of findEmptyPortalShape; false for an empty Optional
bool portal_shape_find_empty_portal_shape(level &        lv,
                                          block_pos      pos,
                                          direction_axis preferred_axis,
                                          portal_shape & out) {
    direction_axis axes[2] = { preferred_axis,
                               preferred_axis == DIRECTION_AXIS_X ? DIRECTION_AXIS_Z : DIRECTION_AXIS_X };
    for (direction_axis axis : axes) {
        out = portal_shape_find_any_shape(lv, pos, axis);
        if (portal_shape_is_valid(out) && out.num_portal_blocks == 0) {
            return true;
        }
    }
    return false;
}

bool portal_shape_is_complete(const portal_shape & s) {
    return portal_shape_is_valid(s) && s.num_portal_blocks == s.width * s.height;
}

// BlockPos.betweenClosed walks x fastest, then y, then z
void portal_shape_create_portal_blocks(level & lv, const portal_shape & s) {
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
