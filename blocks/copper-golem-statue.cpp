// CopperGolemStatueBlock and WeatheringCopperGolemStatueBlock: a use steps the pose, which comparators read

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"

// Pose.getNextPose: BY_ID maps past STAR back to STANDING
static void copper_golem_statue_update_pose(level & lv, state_id st, block_pos pos) {
    int32_t pose = (state_get(st, PROP_COPPER_GOLEM_POSE) + 1) % 4;
    level_set_block(lv, pos, state_set(st, PROP_COPPER_GOLEM_POSE, pose), UPDATE_FLAG_ALL);
}

int32_t copper_golem_statue_get_analog_output_signal(state_id st) {
    return state_get(st, PROP_COPPER_GOLEM_POSE) + 1;
}

state_id copper_golem_statue_update_shape(level & lv, state_id st) {
    if (state_get(st, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
    }
    return st;
}

interaction_result copper_golem_statue_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack) {
    if (item_defs[stack.item].tags & ITEM_TAG_AXES) {
        return INTERACTION_RESULT_PASS;
    }
    copper_golem_statue_update_pose(lv, st, pos);
    return INTERACTION_RESULT_SUCCESS;
}

// an axe turns only the unaffected statue back into a golem; honeycomb is left to HoneycombItem.useOn
interaction_result weathering_copper_golem_statue_use_item_on(level &            lv,
                                                              state_id           st,
                                                              block_pos          pos,
                                                              const item_stack & stack) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || be->type != BLOCK_ENTITY_TYPE_COPPER_GOLEM_STATUE) {
        return INTERACTION_RESULT_PASS;
    }
    if (!(item_defs[stack.item].tags & ITEM_TAG_AXES)) {
        if (stack.item == ITEM_HONEYCOMB) {
            return INTERACTION_RESULT_PASS;
        }
        copper_golem_statue_update_pose(lv, st, pos);
        return INTERACTION_RESULT_SUCCESS;
    }
    if (!state_is(st, BLOCK_COPPER_GOLEM_STATUE)) {
        return INTERACTION_RESULT_PASS;
    }
    // CopperGolemStatueBlockEntity.removeStatue draws nothing from the level random
    trace_boundary(lv.tr, "spawn copper_golem %d %d %d", pos.x, pos.y, pos.z);
    return INTERACTION_RESULT_SUCCESS;
}
