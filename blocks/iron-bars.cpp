// IronBarsBlock (bars and panes): each horizontal side connects to a sturdy face, other bars or a wall

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

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
    return state_set(st, cross_collision_property_by_direction(dir), iron_bars_attachs_to(neighbor, face_solid));
}

state_id iron_bars_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    block_pos pos   = block_place_context_get_clicked_pos(ctx);
    bool      water = block_place_context_is_water_source(lv, ctx);
    state_id  st    = block_default(block);
    for (direction dir : DIRECTION_HORIZONTAL) {
        block_pos neighbor_pos = pos_relative(pos, dir);
        state_id  neighbor     = level_get_state(lv, neighbor_pos);
        bool face_solid = block_is_face_sturdy(lv, neighbor, neighbor_pos, direction_opposite(dir), SUPPORT_TYPE_FULL);
        st = state_set(st, cross_collision_property_by_direction(dir), iron_bars_attachs_to(neighbor, face_solid));
    }
    return state_set(st, PROP_WATERLOGGED, water);
}
