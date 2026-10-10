// ConcretePowderBlock: becomes concrete where it touches water. The landing of a falling powder is an entity matter.

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

// the face test runs on the neighbor's state but with the powder's position, as vanilla passes it
static bool concrete_powder_touches_liquid(level & lv, block_pos pos) {
    for (int32_t dir = DIRECTION_DOWN; dir <= DIRECTION_EAST; dir++) {
        if (dir == DIRECTION_DOWN && !state_has(level_get_state(lv, pos), STATE_FLAG_WATER)) {
            continue;
        }
        state_id neighbor = level_get_state(lv, pos_relative(pos, (direction) dir));
        if (state_has(neighbor, STATE_FLAG_WATER) &&
            !block_is_face_sturdy(lv, neighbor, pos, direction_opposite((direction) dir), SUPPORT_TYPE_FULL)) {
            return true;
        }
    }
    return false;
}

static state_id concrete_powder_concrete(int32_t block) {
    return block_default(block_field_get(block, FIELD_CONCRETE_POWDER_BLOCK_CONCRETE));
}

state_id concrete_powder_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    block_pos pos = block_place_context_get_clicked_pos(ctx);
    if (state_has(level_get_state(lv, pos), STATE_FLAG_WATER) || concrete_powder_touches_liquid(lv, pos)) {
        return concrete_powder_concrete(block);
    }
    return block_default(block);
}

state_id concrete_powder_update_shape(level & lv, state_id st, block_pos pos) {
    if (concrete_powder_touches_liquid(lv, pos)) {
        return concrete_powder_concrete(state_block(st));
    }
    return falling_block_update_shape(lv, st, pos);
}
