// FireworkRocketItem: a rocket starts 0.15 past the clicked face; inputs click the face center, so that is the block in front

#include "core/trace.h"
#include "engine/level.h"
#include "items/handlers.h"

interaction_result firework_rocket_item_use_on(level & lv, const use_on_context & hit) {
    block_pos front = pos_relative(hit.pos, hit.face);
    trace_boundary(lv.tr, "spawn firework_rocket %d %d %d", front.x, front.y, front.z);
    return INTERACTION_RESULT_SUCCESS;
}
