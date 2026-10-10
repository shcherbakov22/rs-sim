// BaseFireBlock: shared fire logic; fire inside an empty obsidian frame lights a nether portal instead

#include "blocks/handlers.h"
#include "engine/level.h"

// soul fire on soul sand or soil, else plain fire
state_id base_fire_get_state(level & lv, block_pos pos) {
    state_id below = level_get_state(lv, pos_relative(pos, DIRECTION_DOWN));
    return soul_fire_can_survive_on_block(below) ? block_default(BLOCK_SOUL_FIRE) :
                                                   fire_get_state_for_placement(lv, pos);
}

// the oracle level is the overworld, a portal dimension
void base_fire_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    if (state_is(old, state_block(st))) {
        return;
    }
    portal_shape shape;
    if (portal_shape_find_empty_portal_shape(lv, pos, DIRECTION_AXIS_X, shape)) {
        portal_shape_create_portal_blocks(lv, shape);
        return;
    }
    if (!block_can_survive(lv, st, pos)) {
        level_remove_block(lv, pos, false);
    }
}

// the forward direction is a player's horizontal direction, so vanilla's random axis for a vertical one never runs
static bool base_fire_is_portal(level & lv, block_pos pos, direction forward) {
    bool has_obsidian = false;
    for (int32_t d = 0; d < 6; d++) {
        if (state_is(level_get_state(lv, pos_relative(pos, (direction) d)), BLOCK_OBSIDIAN)) {
            has_obsidian = true;
            break;
        }
    }
    if (!has_obsidian) {
        return false;
    }
    portal_shape shape;
    return portal_shape_find_empty_portal_shape(lv, pos, direction_get_axis(direction_counter_clockwise(forward)),
                                                shape);
}

bool base_fire_can_be_placed_at(level & lv, block_pos pos, direction forward) {
    if (!state_has(level_get_state(lv, pos), STATE_FLAG_AIR)) {
        return false;
    }
    return block_can_survive(lv, base_fire_get_state(lv, pos), pos) || base_fire_is_portal(lv, pos, forward);
}
