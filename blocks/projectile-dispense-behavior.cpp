// ProjectileDispenseBehavior: the projectile spawns at the position function of its item's DispenseConfig

#include "blocks/handlers.h"
#include "core/rng.h"
#include "core/trace.h"
#include "engine/level.h"

#include <cmath>

item_stack projectile_dispense_behavior_execute(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    const block_pos & step     = direction_defs[dispenser_facing(st)].step;
    int32_t           item     = item_stack_get_item(dispensed);
    // the default config: DispenserBlock.getDispensePosition with scale 0.7 and offset (0, 0.1, 0)
    double            scale    = 0.7;
    double            offset_y = 0.1;
    switch (item_defs[item].cls) {
        case ICLASS_FIREWORK_ROCKET_ITEM:
            // FireworkRocketItem.getEntityJustOutsideOfBlockPos
            scale    = 0.5000099999997474;
            offset_y = 0.0;
            break;
        case ICLASS_FIRE_CHARGE_ITEM:
        case ICLASS_WIND_CHARGE_ITEM:
            scale    = 1.0;
            offset_y = 0.0;
            break;
    }
    double x = pos.x + 0.5 + (scale * step.x);
    double y = pos.y + 0.5 + (scale * step.y + offset_y);
    double z = pos.z + 0.5 + (scale * step.z);
    // asProjectile of a fire or wind charge draws three triangle distributed direction components
    if (item_defs[item].cls == ICLASS_FIRE_CHARGE_ITEM || item_defs[item].cls == ICLASS_WIND_CHARGE_ITEM) {
        for (int32_t i = 0; i < 6; i++) {
            rng_next_double(lv.random);
        }
    }
    trace_boundary(lv.tr, "spawn %s %d %d %d", item_defs[item].dispense_entity, (int32_t) std::floor(x),
                   (int32_t) std::floor(y), (int32_t) std::floor(z));
    return dispensed;
}
