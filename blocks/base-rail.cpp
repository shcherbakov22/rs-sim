// BaseRailBlock: rails need rigid support below (and beside, for slopes) and reshape through RailState

#include "blocks/handlers.h"
#include "engine/level.h"

bool base_rail_is_rail(state_id st) {
    return state_has(st, STATE_FLAG_TAG_RAILS) && block_instanceof(state_block(st), VCLASS_BASE_RAIL_BLOCK);
}

bool base_rail_is_straight(state_id st) {
    return block_field_get(state_block(st), FIELD_BASE_RAIL_BLOCK_IS_STRAIGHT) != 0;
}

// RailBlock has the full shape property; the straight rails one without curves
prop_id base_rail_get_shape_property(state_id st) {
    return block_instanceof(state_block(st), VCLASS_RAIL_BLOCK) ? PROP_RAIL_SHAPE : PROP_RAIL_SHAPE_STRAIGHT;
}

static bool base_rail_can_support_rigid_block(level & lv, block_pos pos) {
    return block_is_face_sturdy(lv, level_get_state(lv, pos), pos, DIRECTION_UP, SUPPORT_TYPE_RIGID);
}

static bool base_rail_is_slope(int32_t shape) {
    return shape >= RAIL_SHAPE_ASCENDING_EAST && shape <= RAIL_SHAPE_ASCENDING_SOUTH;
}

bool base_rail_can_survive(level & lv, block_pos pos) {
    return base_rail_can_support_rigid_block(lv, pos_relative(pos, DIRECTION_DOWN));
}

state_id base_rail_update_dir(level & lv, block_pos pos, state_id st, bool first) {
    rail_state rs = rail_state_init(st, pos);
    rail_state_place(lv, rs, level_has_neighbor_signal(lv, pos), first,
                     state_get(st, base_rail_get_shape_property(st)));
    return rs.state;
}

// straight rails then get a neighborChanged of their own, which powers them
state_id base_rail_update_state(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    st = base_rail_update_dir(lv, pos, st, true);
    if (base_rail_is_straight(st)) {
        level_neighbor_changed(lv, st, pos, state_block(st), moved_by_piston);
    }
    return st;
}

void base_rail_on_place(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston) {
    if (state_block(old) != state_block(st)) {
        base_rail_update_state(lv, st, pos, moved_by_piston);
    }
}

static bool base_rail_should_be_removed(level & lv, block_pos pos, int32_t shape) {
    if (!base_rail_can_support_rigid_block(lv, pos_relative(pos, DIRECTION_DOWN))) {
        return true;
    }
    switch (shape) {
        case RAIL_SHAPE_ASCENDING_EAST:
            return !base_rail_can_support_rigid_block(lv, pos_relative(pos, DIRECTION_EAST));
        case RAIL_SHAPE_ASCENDING_WEST:
            return !base_rail_can_support_rigid_block(lv, pos_relative(pos, DIRECTION_WEST));
        case RAIL_SHAPE_ASCENDING_NORTH:
            return !base_rail_can_support_rigid_block(lv, pos_relative(pos, DIRECTION_NORTH));
        case RAIL_SHAPE_ASCENDING_SOUTH:
            return !base_rail_can_support_rigid_block(lv, pos_relative(pos, DIRECTION_SOUTH));
        default:
            return false;
    }
}

// the subclass updateState(state, level, pos, block) runs only while the rail is still there and supported
void base_rail_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block, bool moved_by_piston) {
    if (!state_is(level_get_state(lv, pos), state_block(st))) {
        return;
    }
    if (base_rail_should_be_removed(lv, pos, state_get(st, base_rail_get_shape_property(st)))) {
        level_drop_resources(lv, st, pos, nullptr);
        level_remove_block(lv, pos, moved_by_piston);
        return;
    }
    if (block_instanceof(state_block(st), VCLASS_RAIL_BLOCK)) {
        rail_update_state(lv, st, pos, block);
    } else if (block_instanceof(state_block(st), VCLASS_POWERED_RAIL_BLOCK)) {
        powered_rail_update_state(lv, st, pos);
    }
}

void base_rail_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (moved_by_piston) {
        return;
    }
    if (base_rail_is_slope(state_get(st, base_rail_get_shape_property(st)))) {
        level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_UP), state_block(st));
    }
    if (base_rail_is_straight(st)) {
        level_update_neighbors_at(lv, pos, state_block(st));
        level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_DOWN), state_block(st));
    }
}
