// MinecartItem: a minecart goes on a rail. The slope lift of half a block keeps it in the rail's block.

#include "core/trace.h"
#include "engine/level.h"
#include "items/handlers.h"

// item_def.dispense_entity is the minecart type that MinecartItem and its dispense behaviour share
interaction_result minecart_item_use_on(level & lv, const use_on_context & hit) {
    if (!state_has(level_get_state(lv, hit.pos), STATE_FLAG_TAG_RAILS)) {
        return INTERACTION_RESULT_FAIL;
    }
    trace_boundary(lv.tr, "spawn %s %d %d %d", item_defs[item_stack_get_item(hit.stack)].dispense_entity, hit.pos.x,
                   hit.pos.y, hit.pos.z);
    return INTERACTION_RESULT_SUCCESS;
}
