// RailState of a straight rail: a rail and the positions it connects to; vanilla builds a fresh one for every query

#include "blocks/handlers.h"
#include "engine/level.h"

static void rail_state_update_connections(rail_state & rs, int32_t shape) {
    block_pos p = rs.pos;
    rs.connections.clear();
    switch (shape) {
        case RAIL_SHAPE_NORTH_SOUTH:
            rs.connections = { pos_relative(p, DIRECTION_NORTH), pos_relative(p, DIRECTION_SOUTH) };
            break;
        case RAIL_SHAPE_EAST_WEST:
            rs.connections = { pos_relative(p, DIRECTION_WEST), pos_relative(p, DIRECTION_EAST) };
            break;
        case RAIL_SHAPE_ASCENDING_EAST:
            rs.connections = { pos_relative(p, DIRECTION_WEST),
                               pos_relative(pos_relative(p, DIRECTION_EAST), DIRECTION_UP) };
            break;
        case RAIL_SHAPE_ASCENDING_WEST:
            rs.connections = { pos_relative(pos_relative(p, DIRECTION_WEST), DIRECTION_UP),
                               pos_relative(p, DIRECTION_EAST) };
            break;
        case RAIL_SHAPE_ASCENDING_NORTH:
            rs.connections = { pos_relative(pos_relative(p, DIRECTION_NORTH), DIRECTION_UP),
                               pos_relative(p, DIRECTION_SOUTH) };
            break;
        case RAIL_SHAPE_ASCENDING_SOUTH:
            rs.connections = { pos_relative(p, DIRECTION_NORTH),
                               pos_relative(pos_relative(p, DIRECTION_SOUTH), DIRECTION_UP) };
            break;
    }
}

rail_state rail_state_init(state_id st, block_pos pos) {
    rail_state rs = {};
    rs.pos        = pos;
    rs.state      = st;
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

static bool rail_state_connects_to(const rail_state & rs, const rail_state & rail) {
    return rail_state_has_connection(rs, rail.pos);
}

// keep only the connections whose rail connects back, moved to that rail's height
static void rail_state_remove_soft_connections(level & lv, rail_state & rs) {
    for (size_t i = 0; i < rs.connections.size(); i++) {
        rail_state rail;
        if (rail_state_get_rail(lv, rs.connections[i], rail) && rail_state_connects_to(rail, rs)) {
            rs.connections[i] = rail.pos;
        } else {
            rs.connections.erase(rs.connections.begin() + (std::ptrdiff_t) i--);
        }
    }
}

static bool rail_state_can_connect_to(const rail_state & rs, const rail_state & rail) {
    return rail_state_connects_to(rs, rail) || rs.connections.size() != 2;
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
void rail_state_place(level & lv, rail_state & rs, bool first, int32_t default_shape) {
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
