// NetherPortalBlock: a portal block breaks when an update along its plane finds the frame no longer complete

#include "blocks/handlers.h"

// updates across the portal plane leave it alone
state_id nether_portal_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor) {
    direction_axis update_axis = direction_get_axis(dir);
    direction_axis axis        = (direction_axis) state_get(st, PROP_HORIZONTAL_AXIS);
    bool           wrong_axis  = axis != update_axis && update_axis != DIRECTION_AXIS_Y;
    if (!wrong_axis && !state_is(neighbor, BLOCK_NETHER_PORTAL) &&
        !portal_shape_is_complete(portal_shape_find_any_shape(lv, pos, axis))) {
        return STATE_AIR;
    }
    return st;
}
