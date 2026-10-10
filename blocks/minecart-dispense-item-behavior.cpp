// MinecartDispenseItemBehavior: a minecart goes on a rail in front, or on a rail below an empty front; else the item drops

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"

#include <cmath>

// getRailShape(...).isSlope; every block in the rails tag is a BaseRailBlock
static bool minecart_dispense_item_behavior_is_slope(state_id st) {
    return base_rail_is_slope(state_get(st, base_rail_get_shape_property(st)));
}

// AbstractMinecart.createMinecart draws nothing from the level random
item_stack minecart_dispense_item_behavior_execute(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    direction         facing  = dispenser_facing(st);
    const block_pos & step    = direction_defs[facing].step;
    double            spawn_x = pos.x + 0.5 + step.x * 1.125;
    double            spawn_y = std::floor(pos.y + 0.5) + step.y;
    double            spawn_z = pos.z + 0.5 + step.z * 1.125;
    block_pos         front   = pos_relative(pos, facing);
    state_id          at      = level_get_state(lv, front);
    double            y_offset;
    if (state_has(at, STATE_FLAG_TAG_RAILS)) {
        y_offset = minecart_dispense_item_behavior_is_slope(at) ? 0.6 : 0.1;
    } else {
        state_id below = level_get_state(lv, pos_relative(front, DIRECTION_DOWN));
        if (!state_has(at, STATE_FLAG_AIR) || !state_has(below, STATE_FLAG_TAG_RAILS)) {
            return default_dispense_item_behavior_dispense(lv, st, pos, dispensed);
        }
        y_offset = facing != DIRECTION_DOWN && minecart_dispense_item_behavior_is_slope(below) ? -0.4 : -0.9;
    }
    trace_boundary(lv.tr, "spawn %s %d %d %d", item_defs[item_stack_get_item(dispensed)].dispense_entity,
                   (int32_t) std::floor(spawn_x), (int32_t) std::floor(spawn_y + y_offset),
                   (int32_t) std::floor(spawn_z));
    return dispensed;
}
