// IronBarsBlock (bars and panes): each horizontal side connects to a sturdy face, other bars or a wall

#include "blocks/handlers.h"
#include "engine/level.h"

// CrossCollisionBlock.PROPERTY_BY_DIRECTION
static prop_id iron_bars_property_by_direction(direction dir) {
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

static bool iron_bars_attachs_to(state_id st, bool face_solid) {
    return (!state_has(st, STATE_FLAG_CONNECTION_EXCEPTION) && face_solid) ||
           block_instanceof(state_block(st), VCLASS_IRON_BARS_BLOCK) || state_has(st, STATE_FLAG_TAG_WALLS);
}

state_id iron_bars_update_shape(level & lv, state_id st, direction dir, block_pos neighbor_pos, state_id neighbor) {
    if (state_get(st, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
    }
    if (!direction_is_horizontal(dir)) {
        return st;
    }
    bool face_solid = block_is_face_sturdy(lv, neighbor, neighbor_pos, direction_opposite(dir), SUPPORT_TYPE_FULL);
    return state_set(st, iron_bars_property_by_direction(dir), iron_bars_attachs_to(neighbor, face_solid));
}
