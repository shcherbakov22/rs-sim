// Rails: rails need rigid support below (and beside, for slopes) and reshape through RailState. Only the straight
// PoweredRailBlock rails (powered and activator) are ported; RailBlock and DetectorRailBlock stop the run.

#include "blocks/handlers.h"
#include "engine/level.h"

// RailShape of a straight rail: its axis, and the side a slope climbs toward

static bool rail_shape_is_north_south(int32_t shape) {
    return shape == RAIL_SHAPE_NORTH_SOUTH || shape == RAIL_SHAPE_ASCENDING_NORTH ||
           shape == RAIL_SHAPE_ASCENDING_SOUTH;
}

static direction rail_shape_ascending_direction(int32_t shape) {
    static constexpr direction ASCENDING[4] = { DIRECTION_EAST, DIRECTION_WEST, DIRECTION_NORTH, DIRECTION_SOUTH };
    return ASCENDING[shape - RAIL_SHAPE_ASCENDING_EAST];
}

bool base_rail_is_slope(int32_t shape) {
    return shape >= RAIL_SHAPE_ASCENDING_EAST && shape <= RAIL_SHAPE_ASCENDING_SOUTH;
}

bool base_rail_is_rail(level & lv, state_id st) {
    int32_t block = state_block(st);
    if (!state_has(st, STATE_FLAG_TAG_RAILS) || !block_instanceof(block, VCLASS_BASE_RAIL_BLOCK)) {
        return false;
    }
    if (!block_instanceof(block, VCLASS_POWERED_RAIL_BLOCK)) {
        level_unsupported(lv, "%s", block_defs[block].name);
        return false;
    }
    return true;
}

// RailState: a rail and the positions it connects to; vanilla builds a fresh one for every query

struct rail_state {
    block_pos              pos;
    state_id               state;
    std::vector<block_pos> connections;
};

// north before south, west before east; a slope raises the side it climbs toward
static void rail_state_update_connections(rail_state & rs, int32_t shape) {
    bool      ns    = rail_shape_is_north_south(shape);
    direction first = ns ? DIRECTION_NORTH : DIRECTION_WEST;
    rs.connections  = { pos_relative(rs.pos, first), pos_relative(rs.pos, direction_opposite(first)) };
    if (base_rail_is_slope(shape)) {
        block_pos & high = rs.connections[rail_shape_ascending_direction(shape) == first ? 0 : 1];
        high             = pos_relative(high, DIRECTION_UP);
    }
}

static rail_state rail_state_init(state_id st, block_pos pos) {
    rail_state rs = { pos, st, {} };
    rail_state_update_connections(rs, state_get(st, PROP_RAIL_SHAPE_STRAIGHT));
    return rs;
}

// the rail at pos, else above, else below
static bool rail_state_get_rail(level & lv, block_pos pos, rail_state & out) {
    for (block_pos test : { pos, pos_relative(pos, DIRECTION_UP), pos_relative(pos, DIRECTION_DOWN) }) {
        state_id st = level_get_state(lv, test);
        if (base_rail_is_rail(lv, st)) {
            out = rail_state_init(st, test);
            return true;
        }
    }
    return false;
}

// connections match by column, whatever their height
static bool rail_state_has_connection(const rail_state & rs, block_pos rail_pos) {
    for (block_pos p : rs.connections) {
        if (p.x == rail_pos.x && p.z == rail_pos.z) {
            return true;
        }
    }
    return false;
}

// keep only the connections whose rail connects back, moved to that rail's height
static void rail_state_remove_soft_connections(level & lv, rail_state & rs) {
    for (size_t i = 0; i < rs.connections.size(); i++) {
        rail_state rail;
        if (rail_state_get_rail(lv, rs.connections[i], rail) && rail_state_has_connection(rail, rs.pos)) {
            rs.connections[i] = rail.pos;
        } else {
            rs.connections.erase(rs.connections.begin() + (std::ptrdiff_t) i--);
        }
    }
}

static bool rail_state_can_connect_to(const rail_state & rs, const rail_state & rail) {
    return rail_state_has_connection(rs, rail.pos) || rs.connections.size() != 2;
}

static bool rail_state_is_rail_above(level & lv, block_pos pos, direction dir) {
    return base_rail_is_rail(lv, level_get_state(lv, pos_relative(pos_relative(pos, dir), DIRECTION_UP)));
}

// a shape that goes north-south or east-west climbs toward a rail one block up
static int32_t rail_state_ascend(level & lv, block_pos pos, int32_t shape) {
    if (shape == RAIL_SHAPE_NORTH_SOUTH) {
        if (rail_state_is_rail_above(lv, pos, DIRECTION_NORTH)) {
            shape = RAIL_SHAPE_ASCENDING_NORTH;
        }
        if (rail_state_is_rail_above(lv, pos, DIRECTION_SOUTH)) {
            shape = RAIL_SHAPE_ASCENDING_SOUTH;
        }
    }
    if (shape == RAIL_SHAPE_EAST_WEST) {
        if (rail_state_is_rail_above(lv, pos, DIRECTION_EAST)) {
            shape = RAIL_SHAPE_ASCENDING_EAST;
        }
        if (rail_state_is_rail_above(lv, pos, DIRECTION_WEST)) {
            shape = RAIL_SHAPE_ASCENDING_WEST;
        }
    }
    return shape;
}

static void rail_state_connect_to(level & lv, rail_state & rs, const rail_state & rail) {
    rs.connections.push_back(rail.pos);
    bool    n     = rail_state_has_connection(rs, pos_relative(rs.pos, DIRECTION_NORTH));
    bool    s     = rail_state_has_connection(rs, pos_relative(rs.pos, DIRECTION_SOUTH));
    bool    w     = rail_state_has_connection(rs, pos_relative(rs.pos, DIRECTION_WEST));
    bool    e     = rail_state_has_connection(rs, pos_relative(rs.pos, DIRECTION_EAST));
    int32_t shape = -1;
    if (n || s) {
        shape = RAIL_SHAPE_NORTH_SOUTH;
    }
    if (w || e) {
        shape = RAIL_SHAPE_EAST_WEST;
    }
    shape = rail_state_ascend(lv, rs.pos, shape);
    if (shape == -1) {
        shape = RAIL_SHAPE_NORTH_SOUTH;
    }
    rs.state = state_set(rs.state, PROP_RAIL_SHAPE_STRAIGHT, shape);
    level_set_block(lv, rs.pos, rs.state, UPDATE_FLAG_ALL);
}

static bool rail_state_has_neighbor_rail(level & lv, const rail_state & rs, block_pos rail_pos) {
    rail_state neighbor;
    if (!rail_state_get_rail(lv, rail_pos, neighbor)) {
        return false;
    }
    rail_state_remove_soft_connections(lv, neighbor);
    return rail_state_can_connect_to(neighbor, rs);
}

// a rail with neighbors both along and across its axis keeps its shape
static void rail_state_place(level & lv, rail_state & rs, bool first, int32_t default_shape) {
    bool    n              = rail_state_has_neighbor_rail(lv, rs, pos_relative(rs.pos, DIRECTION_NORTH));
    bool    s              = rail_state_has_neighbor_rail(lv, rs, pos_relative(rs.pos, DIRECTION_SOUTH));
    bool    w              = rail_state_has_neighbor_rail(lv, rs, pos_relative(rs.pos, DIRECTION_WEST));
    bool    e              = rail_state_has_neighbor_rail(lv, rs, pos_relative(rs.pos, DIRECTION_EAST));
    int32_t shape          = -1;
    bool    north_or_south = n || s;
    bool    west_or_east   = w || e;
    if (north_or_south && !west_or_east) {
        shape = RAIL_SHAPE_NORTH_SOUTH;
    }
    if (west_or_east && !north_or_south) {
        shape = RAIL_SHAPE_EAST_WEST;
    }
    if (north_or_south && west_or_east) {
        shape = default_shape;
    }
    shape = rail_state_ascend(lv, rs.pos, shape);
    if (shape == -1) {
        shape = default_shape;
    }
    rail_state_update_connections(rs, shape);
    rs.state = state_set(rs.state, PROP_RAIL_SHAPE_STRAIGHT, shape);
    if (!first && level_get_state(lv, rs.pos) == rs.state) {
        return;
    }
    level_set_block(lv, rs.pos, rs.state, UPDATE_FLAG_ALL);
    for (size_t i = 0; i < rs.connections.size(); i++) {
        rail_state neighbor;
        if (rail_state_get_rail(lv, rs.connections[i], neighbor)) {
            rail_state_remove_soft_connections(lv, neighbor);
            if (rail_state_can_connect_to(neighbor, rs)) {
                rail_state_connect_to(lv, neighbor, rs);
            }
        }
    }
}

// PoweredRailBlock (powered and activator rails): power passes along up to 8 rails of the same block

static bool powered_rail_find_powered_rail_signal(level &   lv,
                                                  block_pos pos,
                                                  state_id  st,
                                                  bool      forward,
                                                  int32_t   search_depth);

// a rail across the search axis breaks the line
static bool powered_rail_is_same_rail_with_power(level &   lv,
                                                 block_pos pos,
                                                 state_id  from,
                                                 bool      forward,
                                                 int32_t   search_depth,
                                                 bool      north_south) {
    state_id st = level_get_state(lv, pos);
    if (!state_is(st, state_block(from)) ||
        rail_shape_is_north_south(state_get(st, PROP_RAIL_SHAPE_STRAIGHT)) != north_south ||
        !state_get(st, PROP_POWERED)) {
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
    int32_t   shape = state_get(st, PROP_RAIL_SHAPE_STRAIGHT);
    bool      ns    = rail_shape_is_north_south(shape);
    direction step  = ns ? (forward ? DIRECTION_SOUTH : DIRECTION_NORTH) : (forward ? DIRECTION_WEST : DIRECTION_EAST);
    block_pos next  = pos_relative(pos, step);
    bool      up    = base_rail_is_slope(shape) && rail_shape_ascending_direction(shape) == step;
    if (up) {
        next = pos_relative(next, DIRECTION_UP);
    }
    if (powered_rail_is_same_rail_with_power(lv, next, st, forward, search_depth, ns)) {
        return true;
    }
    return !up &&
           powered_rail_is_same_rail_with_power(lv, pos_relative(next, DIRECTION_DOWN), st, forward, search_depth, ns);
}

static void powered_rail_update_state(level & lv, state_id st, block_pos pos) {
    bool should_power = level_has_neighbor_signal(lv, pos) ||
                        powered_rail_find_powered_rail_signal(lv, pos, st, true, 0) ||
                        powered_rail_find_powered_rail_signal(lv, pos, st, false, 0);
    if (should_power == (bool) state_get(st, PROP_POWERED)) {
        return;
    }
    level_set_block(lv, pos, state_set(st, PROP_POWERED, should_power), UPDATE_FLAG_ALL);
    level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_DOWN), state_block(st));
    if (base_rail_is_slope(state_get(st, PROP_RAIL_SHAPE_STRAIGHT))) {
        level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_UP), state_block(st));
    }
}

// BaseRailBlock

static bool base_rail_can_support_rigid_block(level & lv, block_pos pos) {
    return block_is_face_sturdy(lv, level_get_state(lv, pos), pos, DIRECTION_UP, SUPPORT_TYPE_RIGID);
}

// updateDir, then the straight rail gets a neighborChanged of its own, which powers it
static void base_rail_on_place(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston) {
    if (!base_rail_is_rail(lv, st) || state_block(old) == state_block(st)) {
        return;
    }
    rail_state rs = rail_state_init(st, pos);
    rail_state_place(lv, rs, true, state_get(st, PROP_RAIL_SHAPE_STRAIGHT));
    level_neighbor_changed(lv, rs.state, pos, state_block(st), moved_by_piston);
}

static bool base_rail_should_be_removed(level & lv, block_pos pos, int32_t shape) {
    if (!base_rail_can_support_rigid_block(lv, pos_relative(pos, DIRECTION_DOWN))) {
        return true;
    }
    return base_rail_is_slope(shape) &&
           !base_rail_can_support_rigid_block(lv, pos_relative(pos, rail_shape_ascending_direction(shape)));
}

// the subclass updateState(state, level, pos, block) runs only while the rail is still there and supported
static void base_rail_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool moved_by_piston) {
    if (!base_rail_is_rail(lv, st) || !state_is(level_get_state(lv, pos), state_block(st))) {
        return;
    }
    if (base_rail_should_be_removed(lv, pos, state_get(st, PROP_RAIL_SHAPE_STRAIGHT))) {
        level_drop_resources(lv, st, pos, nullptr);
        level_remove_block(lv, pos, moved_by_piston);
        return;
    }
    powered_rail_update_state(lv, st, pos);
}

static void base_rail_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (!base_rail_is_rail(lv, st) || moved_by_piston) {
        return;
    }
    if (base_rail_is_slope(state_get(st, PROP_RAIL_SHAPE_STRAIGHT))) {
        level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_UP), state_block(st));
    }
    level_update_neighbors_at(lv, pos, state_block(st));
    level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_DOWN), state_block(st));
}

// clang-format off
const std::vector<block_hooks> rail_hooks = {
    { .vclass = VCLASS_BASE_RAIL_BLOCK,
      .update_shape = block_behaviour_update_shape,
      .neighbor_changed = base_rail_neighbor_changed,
      .on_place = base_rail_on_place,
      .affect_neighbors_after_removal = base_rail_affect_neighbors_after_removal },
};
// clang-format on
