// IronBarsBlock (bars and panes): each horizontal side connects to a sturdy face, other bars or a wall

#include "blocks/handlers.h"
#include "engine/level.h"

static bool iron_bars_attachs_to(state_id st, bool face_solid) {
    return (!state_has(st, STATE_FLAG_CONNECTION_EXCEPTION) && face_solid) ||
           block_instanceof(state_block(st), VCLASS_IRON_BARS_BLOCK) || state_has(st, STATE_FLAG_TAG_WALLS);
}

state_id iron_bars_update_shape(level & lv, state_id st, direction dir, block_pos neighbor_pos, state_id neighbor) {
    if (!direction_is_horizontal(dir)) {
        return st;
    }
    bool face_solid = block_is_face_sturdy(lv, neighbor, neighbor_pos, direction_opposite(dir), SUPPORT_TYPE_FULL);
    return state_set(st, cross_collision_property_by_direction(dir), iron_bars_attachs_to(neighbor, face_solid));
}
