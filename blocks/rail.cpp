// Rails: BaseRailBlock and RailState; only RailBlock curves, and a detector rail powers while a minecart is on it

#include "blocks/handlers.h"
#include "engine/level.h"
#include "entities/handlers.h"

// RailShape

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

bool base_rail_is_rail(state_id st) {
    return state_has(st, STATE_FLAG_TAG_RAILS) && block_instanceof(state_block(st), VCLASS_BASE_RAIL_BLOCK);
}

static bool base_rail_is_rail_at(level & lv, block_pos pos) { return base_rail_is_rail(level_get_state(lv, pos)); }

static bool base_rail_is_straight(state_id st) {
    return block_field_get(state_block(st), FIELD_BASE_RAIL_BLOCK_IS_STRAIGHT) != 0;
}

// BaseRailBlock.getShapeProperty
static prop_id base_rail_get_shape_property(state_id st) {
    return block_instanceof(state_block(st), VCLASS_RAIL_BLOCK) ? PROP_RAIL_SHAPE : PROP_RAIL_SHAPE_STRAIGHT;
}

int32_t base_rail_get_shape(state_id st) { return state_get(st, base_rail_get_shape_property(st)); }

// RailState: a rail and the positions it connects to; vanilla builds a fresh one for every query

struct rail_state {
    block_pos              pos;
    state_id               state;
    bool                   is_straight;
    std::vector<block_pos> connections;
};

// per RailShape the two sides it connects, and which of them is one block up (-1: neither)
static void rail_state_update_connections(rail_state & rs, int32_t shape) {
    static constexpr direction SIDES[10][2] = {
        { DIRECTION_NORTH, DIRECTION_SOUTH },
        { DIRECTION_WEST,  DIRECTION_EAST  },
        { DIRECTION_WEST,  DIRECTION_EAST  },
        { DIRECTION_WEST,  DIRECTION_EAST  },
        { DIRECTION_NORTH, DIRECTION_SOUTH },
        { DIRECTION_NORTH, DIRECTION_SOUTH },
        { DIRECTION_EAST,  DIRECTION_SOUTH },
        { DIRECTION_WEST,  DIRECTION_SOUTH },
        { DIRECTION_WEST,  DIRECTION_NORTH },
        { DIRECTION_EAST,  DIRECTION_NORTH },
    };
    static constexpr int8_t UP[10] = { -1, -1, 1, 0, 0, 1, -1, -1, -1, -1 };
    rs.connections                 = { pos_relative(rs.pos, SIDES[shape][0]), pos_relative(rs.pos, SIDES[shape][1]) };
    if (UP[shape] >= 0) rs.connections[UP[shape]] = pos_relative(rs.connections[UP[shape]], DIRECTION_UP);
}

static rail_state rail_state_init(state_id st, block_pos pos) {
    rail_state rs = { pos, st, base_rail_is_straight(st), {} };
    rail_state_update_connections(rs, base_rail_get_shape(st));
    return rs;
}

// the rail at pos, else above, else below
static bool rail_state_get_rail(level & lv, block_pos pos, rail_state & out) {
    for (block_pos test : { pos, pos_relative(pos, DIRECTION_UP), pos_relative(pos, DIRECTION_DOWN) }) {
        state_id st = level_get_state(lv, test);
        if (base_rail_is_rail(st)) {
            out = rail_state_init(st, test);
            return true;
        }
    }
    return false;
}

// connections match by column, whatever their height
static bool rail_state_has_connection(const rail_state & rs, block_pos rail_pos) {
    for (block_pos p : rs.connections) {
        if (p.x == rail_pos.x && p.z == rail_pos.z) return true;
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

static bool rail_state_has_rail(level & lv, block_pos pos) {
    return base_rail_is_rail_at(lv, pos) || base_rail_is_rail_at(lv, pos_relative(pos, DIRECTION_UP)) ||
           base_rail_is_rail_at(lv, pos_relative(pos, DIRECTION_DOWN));
}

// Direction.Plane.HORIZONTAL order
static int32_t rail_state_count_potential_connections(level & lv, block_pos pos) {
    int32_t count = 0;
    for (direction dir : { DIRECTION_NORTH, DIRECTION_EAST, DIRECTION_SOUTH, DIRECTION_WEST }) {
        if (rail_state_has_rail(lv, pos_relative(pos, dir))) count++;
    }
    return count;
}

static bool rail_state_can_connect_to(const rail_state & rs, const rail_state & rail) {
    return rail_state_has_connection(rs, rail.pos) || rs.connections.size() != 2;
}

// the curve that joins exactly the two sides set, -1 for none
static int32_t rail_state_curve(bool n, bool s, bool w, bool e) {
    if (s && e && !n && !w) return RAIL_SHAPE_SOUTH_EAST;
    if (s && w && !n && !e) return RAIL_SHAPE_SOUTH_WEST;
    if (n && w && !s && !e) return RAIL_SHAPE_NORTH_WEST;
    if (n && e && !s && !w) return RAIL_SHAPE_NORTH_EAST;
    return -1;
}

// a shape that goes north-south or east-west climbs toward a rail one block up
static int32_t rail_state_ascend(level & lv, block_pos pos, int32_t shape) {
    if (shape == RAIL_SHAPE_NORTH_SOUTH) {
        if (base_rail_is_rail_at(lv, pos_relative(pos_relative(pos, DIRECTION_NORTH), DIRECTION_UP))) {
            shape = RAIL_SHAPE_ASCENDING_NORTH;
        }
        if (base_rail_is_rail_at(lv, pos_relative(pos_relative(pos, DIRECTION_SOUTH), DIRECTION_UP))) {
            shape = RAIL_SHAPE_ASCENDING_SOUTH;
        }
    }
    if (shape == RAIL_SHAPE_EAST_WEST) {
        if (base_rail_is_rail_at(lv, pos_relative(pos_relative(pos, DIRECTION_EAST), DIRECTION_UP))) {
            shape = RAIL_SHAPE_ASCENDING_EAST;
        }
        if (base_rail_is_rail_at(lv, pos_relative(pos_relative(pos, DIRECTION_WEST), DIRECTION_UP))) {
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
    if (n || s) shape = RAIL_SHAPE_NORTH_SOUTH;
    if (w || e) shape = RAIL_SHAPE_EAST_WEST;
    int32_t curve = rs.is_straight ? -1 : rail_state_curve(n, s, w, e);
    if (curve != -1) shape = curve;
    shape = rail_state_ascend(lv, rs.pos, shape);
    if (shape == -1) shape = RAIL_SHAPE_NORTH_SOUTH;
    rs.state = state_set(rs.state, base_rail_get_shape_property(rs.state), shape);
    level_set_block(lv, rs.pos, rs.state, UPDATE_FLAG_ALL);
}

static bool rail_state_has_neighbor_rail(level & lv, const rail_state & rs, block_pos rail_pos) {
    rail_state neighbor;
    if (!rail_state_get_rail(lv, rail_pos, neighbor)) return false;
    rail_state_remove_soft_connections(lv, neighbor);
    return rail_state_can_connect_to(neighbor, rs);
}

// a junction of both axes keeps the default shape; a curving rail turns there, and the signal reverses the check order
static int32_t rail_state_junction(bool n, bool s, bool w, bool e, bool has_signal, int32_t shape) {
    if (has_signal) {
        if (s && e) shape = RAIL_SHAPE_SOUTH_EAST;
        if (s && w) shape = RAIL_SHAPE_SOUTH_WEST;
        if (n && e) shape = RAIL_SHAPE_NORTH_EAST;
        if (n && w) shape = RAIL_SHAPE_NORTH_WEST;
    } else {
        if (n && w) shape = RAIL_SHAPE_NORTH_WEST;
        if (n && e) shape = RAIL_SHAPE_NORTH_EAST;
        if (s && w) shape = RAIL_SHAPE_SOUTH_WEST;
        if (s && e) shape = RAIL_SHAPE_SOUTH_EAST;
    }
    return shape;
}

static void rail_state_place(level & lv, rail_state & rs, bool has_signal, bool first, int32_t default_shape) {
    bool    n              = rail_state_has_neighbor_rail(lv, rs, pos_relative(rs.pos, DIRECTION_NORTH));
    bool    s              = rail_state_has_neighbor_rail(lv, rs, pos_relative(rs.pos, DIRECTION_SOUTH));
    bool    w              = rail_state_has_neighbor_rail(lv, rs, pos_relative(rs.pos, DIRECTION_WEST));
    bool    e              = rail_state_has_neighbor_rail(lv, rs, pos_relative(rs.pos, DIRECTION_EAST));
    int32_t shape          = -1;
    bool    north_or_south = n || s;
    bool    west_or_east   = w || e;
    if (north_or_south && !west_or_east) shape = RAIL_SHAPE_NORTH_SOUTH;
    if (west_or_east && !north_or_south) shape = RAIL_SHAPE_EAST_WEST;
    int32_t curve = rs.is_straight ? -1 : rail_state_curve(n, s, w, e);
    if (curve != -1) shape = curve;
    if (shape == -1 && north_or_south && west_or_east) {
        shape = rs.is_straight ? default_shape : rail_state_junction(n, s, w, e, has_signal, default_shape);
    }
    shape = rail_state_ascend(lv, rs.pos, shape);
    if (shape == -1) shape = default_shape;
    rail_state_update_connections(rs, shape);
    rs.state = state_set(rs.state, base_rail_get_shape_property(rs.state), shape);
    if (!first && level_get_state(lv, rs.pos) == rs.state) return;
    level_set_block(lv, rs.pos, rs.state, UPDATE_FLAG_ALL);
    for (size_t i = 0; i < rs.connections.size(); i++) {
        rail_state neighbor;
        if (rail_state_get_rail(lv, rs.connections[i], neighbor)) {
            rail_state_remove_soft_connections(lv, neighbor);
            if (rail_state_can_connect_to(neighbor, rs)) rail_state_connect_to(lv, neighbor, rs);
        }
    }
}

// BaseRailBlock.updateDir
static state_id base_rail_update_dir(level & lv, block_pos pos, state_id st, bool first) {
    bool       has_signal = level_has_neighbor_signal(lv, pos);
    rail_state rs         = rail_state_init(st, pos);
    rail_state_place(lv, rs, has_signal, first, base_rail_get_shape(st));
    return rs.state;
}

// RailBlock.updateState: a new signal source next to a T junction flips its curve
static void rail_update_state(level & lv, state_id st, block_pos pos, int32_t block) {
    if (block_is_signal_source(lv, block_default(block)) && rail_state_count_potential_connections(lv, pos) == 3) {
        base_rail_update_dir(lv, pos, st, false);
    }
}

// PoweredRailBlock (powered and activator rails)

static bool powered_rail_find_powered_rail_signal(level & lv, block_pos pos, state_id st, bool forward,
                                                  int32_t search_depth);

// a rail across the search axis breaks the line
static bool powered_rail_is_same_rail_with_power(level & lv, block_pos pos, state_id from, bool forward,
                                                 int32_t search_depth, bool north_south) {
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
static bool powered_rail_find_powered_rail_signal(level & lv, block_pos pos, state_id st, bool forward,
                                                  int32_t search_depth) {
    if (search_depth >= 8) return false;
    int32_t   shape = state_get(st, PROP_RAIL_SHAPE_STRAIGHT);
    bool      ns    = rail_shape_is_north_south(shape);
    direction step  = ns ? (forward ? DIRECTION_SOUTH : DIRECTION_NORTH) : (forward ? DIRECTION_WEST : DIRECTION_EAST);
    block_pos next  = pos_relative(pos, step);
    bool      up    = base_rail_is_slope(shape) && rail_shape_ascending_direction(shape) == step;
    if (up) next = pos_relative(next, DIRECTION_UP);
    if (powered_rail_is_same_rail_with_power(lv, next, st, forward, search_depth, ns)) return true;
    return !up &&
           powered_rail_is_same_rail_with_power(lv, pos_relative(next, DIRECTION_DOWN), st, forward, search_depth, ns);
}

static void powered_rail_update_state(level & lv, state_id st, block_pos pos) {
    bool should_power = level_has_neighbor_signal(lv, pos) ||
                        powered_rail_find_powered_rail_signal(lv, pos, st, true, 0) ||
                        powered_rail_find_powered_rail_signal(lv, pos, st, false, 0);
    if (should_power == (bool) state_get(st, PROP_POWERED)) return;
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

static bool base_rail_can_survive(level & lv, state_id, block_pos pos) {
    return base_rail_can_support_rigid_block(lv, pos_relative(pos, DIRECTION_DOWN));
}

// updateDir, then a straight rail gets a neighborChanged of its own, which powers it
static state_id base_rail_update_state(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    st = base_rail_update_dir(lv, pos, st, true);
    if (base_rail_is_straight(st)) level_neighbor_changed(lv, st, pos, state_block(st), moved_by_piston);
    return st;
}

static void base_rail_on_place(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston) {
    if (state_block(old) != state_block(st)) base_rail_update_state(lv, st, pos, moved_by_piston);
}

static bool base_rail_should_be_removed(level & lv, block_pos pos, int32_t shape) {
    if (!base_rail_can_support_rigid_block(lv, pos_relative(pos, DIRECTION_DOWN))) return true;
    return base_rail_is_slope(shape) &&
           !base_rail_can_support_rigid_block(lv, pos_relative(pos, rail_shape_ascending_direction(shape)));
}

// the subclass updateState(state, level, pos, block) runs only while the rail is still there and supported.
// DetectorRailBlock keeps the default, which does nothing.
static void base_rail_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block, bool moved_by_piston) {
    if (!state_is(level_get_state(lv, pos), state_block(st))) return;
    if (base_rail_should_be_removed(lv, pos, base_rail_get_shape(st))) {
        level_drop_resources(lv, st, pos, nullptr);
        level_remove_block(lv, pos, moved_by_piston);
    } else if (block_instanceof(state_block(st), VCLASS_POWERED_RAIL_BLOCK)) {
        powered_rail_update_state(lv, st, pos);
    } else if (block_instanceof(state_block(st), VCLASS_RAIL_BLOCK)) {
        rail_update_state(lv, st, pos, block);
    }
}

static void base_rail_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (moved_by_piston) return;
    if (base_rail_is_slope(base_rail_get_shape(st))) {
        level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_UP), state_block(st));
    }
    if (base_rail_is_straight(st)) {
        level_update_neighbors_at(lv, pos, state_block(st));
        level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_DOWN), state_block(st));
    }
}

// DetectorRailBlock

// getInteractingMinecartOfType over getSearchBB, the middle of the block
static void detector_rail_get_interacting_minecarts(level & lv, block_pos pos, bool containers_only,
                                                    std::vector<entity *> & out) {
    vec3                  w = entity_block_to_world(pos);
    std::vector<entity *> found;
    level_get_entities(lv, aabb_of(w.x + 0.2, w.y, w.z + 0.2, w.x + 1 - 0.2, w.y + 1 - 0.2, w.z + 1 - 0.2), found);
    for (entity * e : found) {
        if (entity_is_minecart(*e) && (!containers_only || entity_is_container(*e))) out.push_back(e);
    }
}

static void detector_rail_update_power_to_connected(level & lv, state_id st, block_pos pos) {
    rail_state rs = rail_state_init(st, pos);
    for (block_pos p : rs.connections) {
        state_id connection = level_get_state(lv, p);
        level_neighbor_changed(lv, connection, p, state_block(connection), false);
    }
}

// a pressed rail checks again every 20 ticks
static void detector_rail_check_pressed(level & lv, block_pos pos, state_id st) {
    if (!base_rail_can_survive(lv, st, pos)) return;
    std::vector<entity *> minecarts;
    detector_rail_get_interacting_minecarts(lv, pos, false, minecarts);
    bool should_be_pressed = !minecarts.empty();
    if (should_be_pressed != (state_get(st, PROP_POWERED) != 0)) {
        state_id changed = state_set(st, PROP_POWERED, should_be_pressed);
        level_set_block(lv, pos, changed, UPDATE_FLAG_ALL);
        detector_rail_update_power_to_connected(lv, changed, pos);
        level_update_neighbors_at(lv, pos, state_block(st));
        level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_DOWN), state_block(st));
    }
    if (should_be_pressed) level_schedule_tick(lv, pos, state_block(st), 20);
    level_update_neighbour_for_output_signal(lv, pos, state_block(st));
}

static void detector_rail_on_place(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston) {
    if (state_block(old) == state_block(st)) return;
    detector_rail_check_pressed(lv, pos, base_rail_update_state(lv, st, pos, moved_by_piston));
}

static void detector_rail_entity_inside(level & lv, state_id st, block_pos pos, entity &, entity_effect_collector &,
                                        bool) {
    if (!state_get(st, PROP_POWERED)) detector_rail_check_pressed(lv, pos, st);
}

static void detector_rail_tick(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_POWERED)) detector_rail_check_pressed(lv, pos, st);
}

// the fill of the first container minecart on a pressed rail; no command block minecart is in scope
static int32_t detector_rail_get_analog_output_signal(level & lv, state_id st, block_pos pos, direction) {
    if (!state_get(st, PROP_POWERED)) return 0;
    std::vector<entity *> minecarts;
    detector_rail_get_interacting_minecarts(lv, pos, true, minecarts);
    if (minecarts.empty()) return 0;
    container c;
    c.vehicle = minecarts[0];
    return container_get_redstone_signal(&c);
}

// clang-format off
const std::vector<block_hooks> rail_hooks = {
    { .vclass = VCLASS_BASE_RAIL_BLOCK,
      .update_shape = block_behaviour_update_shape,
      .neighbor_changed = base_rail_neighbor_changed,
      .on_place = base_rail_on_place,
      .affect_neighbors_after_removal = base_rail_affect_neighbors_after_removal,
      .can_survive = base_rail_can_survive },
    { .vclass = VCLASS_DETECTOR_RAIL_BLOCK,
      .on_place = detector_rail_on_place,
      .tick = detector_rail_tick,
      .get_analog_output_signal = detector_rail_get_analog_output_signal,
      .entity_inside = detector_rail_entity_inside },
};
// clang-format on
