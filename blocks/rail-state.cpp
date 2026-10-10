// RailState: a rail and the positions it connects to; vanilla builds a fresh one for every query

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
        case RAIL_SHAPE_SOUTH_EAST:
            rs.connections = { pos_relative(p, DIRECTION_EAST), pos_relative(p, DIRECTION_SOUTH) };
            break;
        case RAIL_SHAPE_SOUTH_WEST:
            rs.connections = { pos_relative(p, DIRECTION_WEST), pos_relative(p, DIRECTION_SOUTH) };
            break;
        case RAIL_SHAPE_NORTH_WEST:
            rs.connections = { pos_relative(p, DIRECTION_WEST), pos_relative(p, DIRECTION_NORTH) };
            break;
        case RAIL_SHAPE_NORTH_EAST:
            rs.connections = { pos_relative(p, DIRECTION_EAST), pos_relative(p, DIRECTION_NORTH) };
            break;
    }
}

rail_state rail_state_init(state_id st, block_pos pos) {
    rail_state rs  = {};
    rs.pos         = pos;
    rs.state       = st;
    rs.is_straight = base_rail_is_straight(st);
    rail_state_update_connections(rs, state_get(st, base_rail_get_shape_property(st)));
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

static bool rail_state_has_rail(level & lv, block_pos pos) {
    return base_rail_is_rail(level_get_state(lv, pos)) ||
           base_rail_is_rail(level_get_state(lv, pos_relative(pos, DIRECTION_UP))) ||
           base_rail_is_rail(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)));
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

int32_t rail_state_count_potential_connections(level & lv, const rail_state & rs) {
    int32_t count = 0;
    for (direction dir : DIRECTION_HORIZONTAL) {
        if (rail_state_has_rail(lv, pos_relative(rs.pos, dir))) {
            count++;
        }
    }
    return count;
}

static bool rail_state_can_connect_to(const rail_state & rs, const rail_state & rail) {
    return rail_state_connects_to(rs, rail) || rs.connections.size() != 2;
}

// a shape that goes north-south or east-west climbs toward a rail one block up
static int32_t rail_state_ascend(level & lv, block_pos pos, int32_t shape) {
    if (shape == RAIL_SHAPE_NORTH_SOUTH) {
        if (base_rail_is_rail(level_get_state(lv, pos_relative(pos_relative(pos, DIRECTION_NORTH), DIRECTION_UP)))) {
            shape = RAIL_SHAPE_ASCENDING_NORTH;
        }
        if (base_rail_is_rail(level_get_state(lv, pos_relative(pos_relative(pos, DIRECTION_SOUTH), DIRECTION_UP)))) {
            shape = RAIL_SHAPE_ASCENDING_SOUTH;
        }
    }
    if (shape == RAIL_SHAPE_EAST_WEST) {
        if (base_rail_is_rail(level_get_state(lv, pos_relative(pos_relative(pos, DIRECTION_EAST), DIRECTION_UP)))) {
            shape = RAIL_SHAPE_ASCENDING_EAST;
        }
        if (base_rail_is_rail(level_get_state(lv, pos_relative(pos_relative(pos, DIRECTION_WEST), DIRECTION_UP)))) {
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
    if (!rs.is_straight) {
        if (s && e && !n && !w) {
            shape = RAIL_SHAPE_SOUTH_EAST;
        }
        if (s && w && !n && !e) {
            shape = RAIL_SHAPE_SOUTH_WEST;
        }
        if (n && w && !s && !e) {
            shape = RAIL_SHAPE_NORTH_WEST;
        }
        if (n && e && !s && !w) {
            shape = RAIL_SHAPE_NORTH_EAST;
        }
    }
    shape = rail_state_ascend(lv, rs.pos, shape);
    if (shape == -1) {
        shape = RAIL_SHAPE_NORTH_SOUTH;
    }
    rs.state = state_set(rs.state, base_rail_get_shape_property(rs.state), shape);
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

// curves win over straight lines; with a signal the south-east corner has the lowest priority, without it the highest
void rail_state_place(level & lv, rail_state & rs, bool has_signal, bool first, int32_t default_shape) {
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
    bool south_and_east = s && e;
    bool south_and_west = s && w;
    bool north_and_east = n && e;
    bool north_and_west = n && w;
    if (!rs.is_straight) {
        if (south_and_east && !n && !w) {
            shape = RAIL_SHAPE_SOUTH_EAST;
        }
        if (south_and_west && !n && !e) {
            shape = RAIL_SHAPE_SOUTH_WEST;
        }
        if (north_and_west && !s && !e) {
            shape = RAIL_SHAPE_NORTH_WEST;
        }
        if (north_and_east && !s && !w) {
            shape = RAIL_SHAPE_NORTH_EAST;
        }
    }
    if (shape == -1) {
        if (north_or_south && west_or_east) {
            shape = default_shape;
        } else if (north_or_south) {
            shape = RAIL_SHAPE_NORTH_SOUTH;
        } else if (west_or_east) {
            shape = RAIL_SHAPE_EAST_WEST;
        }
        if (!rs.is_straight) {
            if (has_signal) {
                if (south_and_east) {
                    shape = RAIL_SHAPE_SOUTH_EAST;
                }
                if (south_and_west) {
                    shape = RAIL_SHAPE_SOUTH_WEST;
                }
                if (north_and_east) {
                    shape = RAIL_SHAPE_NORTH_EAST;
                }
                if (north_and_west) {
                    shape = RAIL_SHAPE_NORTH_WEST;
                }
            } else {
                if (north_and_west) {
                    shape = RAIL_SHAPE_NORTH_WEST;
                }
                if (north_and_east) {
                    shape = RAIL_SHAPE_NORTH_EAST;
                }
                if (south_and_west) {
                    shape = RAIL_SHAPE_SOUTH_WEST;
                }
                if (south_and_east) {
                    shape = RAIL_SHAPE_SOUTH_EAST;
                }
            }
        }
    }
    shape = rail_state_ascend(lv, rs.pos, shape);
    if (shape == -1) {
        shape = default_shape;
    }
    rail_state_update_connections(rs, shape);
    rs.state = state_set(rs.state, base_rail_get_shape_property(rs.state), shape);
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
