// CrossCollisionBlock: the shared side properties of bars, panes and fences

#include "blocks/handlers.h"

// CrossCollisionBlock.PROPERTY_BY_DIRECTION
prop_id cross_collision_property_by_direction(direction dir) {
    switch (dir) {
        case DIRECTION_NORTH:
            return PROP_NORTH;
        case DIRECTION_EAST:
            return PROP_EAST;
        case DIRECTION_SOUTH:
            return PROP_SOUTH;
        default:
            return PROP_WEST;
    }
}
