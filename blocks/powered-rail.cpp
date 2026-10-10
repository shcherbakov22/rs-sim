// PoweredRailBlock (powered and activator rails): power passes along up to 8 rails of the same block

#include "blocks/handlers.h"
#include "engine/level.h"

static bool powered_rail_find_powered_rail_signal(level &   lv,
                                                  block_pos pos,
                                                  state_id  st,
                                                  bool      forward,
                                                  int32_t   search_depth);

static bool powered_rail_is_same_rail_with_power(level &   lv,
                                                 block_pos pos,
                                                 state_id  from,
                                                 bool      forward,
                                                 int32_t   search_depth,
                                                 int32_t   dir) {
    state_id st = level_get_state(lv, pos);
    if (!state_is(st, state_block(from))) {
        return false;
    }
    // a rail across the search axis breaks the line
    int32_t shape = state_get(st, PROP_RAIL_SHAPE_STRAIGHT);
    if (dir == RAIL_SHAPE_EAST_WEST && (shape == RAIL_SHAPE_NORTH_SOUTH || shape == RAIL_SHAPE_ASCENDING_NORTH ||
                                        shape == RAIL_SHAPE_ASCENDING_SOUTH)) {
        return false;
    }
    if (dir == RAIL_SHAPE_NORTH_SOUTH &&
        (shape == RAIL_SHAPE_EAST_WEST || shape == RAIL_SHAPE_ASCENDING_EAST || shape == RAIL_SHAPE_ASCENDING_WEST)) {
        return false;
    }
    if (!state_get(st, PROP_POWERED)) {
        return false;
    }
    return level_has_neighbor_signal(lv, pos) ||
           powered_rail_find_powered_rail_signal(lv, pos, st, forward, search_depth + 1);
}

// forward is south or west; a slope steps up toward its high end and then does not look below
static bool powered_rail_find_powered_rail_signal(level &   lv,
                                                  block_pos pos,
                                                  state_id  st,
                                                  bool      forward,
                                                  int32_t   search_depth) {
    if (search_depth >= 8) {
        return false;
    }
    block_pos next        = pos;
    bool      check_below = true;
    int32_t   shape       = state_get(st, PROP_RAIL_SHAPE_STRAIGHT);
    switch (shape) {
        case RAIL_SHAPE_NORTH_SOUTH:
            next.z += forward ? 1 : -1;
            break;
        case RAIL_SHAPE_EAST_WEST:
            next.x += forward ? -1 : 1;
            break;
        case RAIL_SHAPE_ASCENDING_EAST:
            if (forward) {
                next.x--;
            } else {
                next.x++;
                next.y++;
                check_below = false;
            }
            shape = RAIL_SHAPE_EAST_WEST;
            break;
        case RAIL_SHAPE_ASCENDING_WEST:
            if (forward) {
                next.x--;
                next.y++;
                check_below = false;
            } else {
                next.x++;
            }
            shape = RAIL_SHAPE_EAST_WEST;
            break;
        case RAIL_SHAPE_ASCENDING_NORTH:
            if (forward) {
                next.z++;
            } else {
                next.z--;
                next.y++;
                check_below = false;
            }
            shape = RAIL_SHAPE_NORTH_SOUTH;
            break;
        case RAIL_SHAPE_ASCENDING_SOUTH:
            if (forward) {
                next.z++;
                next.y++;
                check_below = false;
            } else {
                next.z--;
            }
            shape = RAIL_SHAPE_NORTH_SOUTH;
            break;
    }
    if (powered_rail_is_same_rail_with_power(lv, next, st, forward, search_depth, shape)) {
        return true;
    }
    return check_below && powered_rail_is_same_rail_with_power(lv, pos_relative(next, DIRECTION_DOWN), st, forward,
                                                               search_depth, shape);
}

void powered_rail_update_state(level & lv, state_id st, block_pos pos) {
    bool should_power = level_has_neighbor_signal(lv, pos) ||
                        powered_rail_find_powered_rail_signal(lv, pos, st, true, 0) ||
                        powered_rail_find_powered_rail_signal(lv, pos, st, false, 0);
    if (should_power == (bool) state_get(st, PROP_POWERED)) {
        return;
    }
    level_set_block(lv, pos, state_set(st, PROP_POWERED, should_power), UPDATE_FLAG_ALL);
    level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_DOWN), state_block(st));
    int32_t shape = state_get(st, PROP_RAIL_SHAPE_STRAIGHT);
    if (shape >= RAIL_SHAPE_ASCENDING_EAST && shape <= RAIL_SHAPE_ASCENDING_SOUTH) {
        level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_UP), state_block(st));
    }
}
