// VehicleEntity: the damage that breaks minecarts and boats into their item; ContainerEntity: the contents of the chest
// and hopper minecarts and the chest boats.

#include "blocks/handlers.h"
#include "engine/level.h"
#include "entities/handlers.h"

bool entity_is_minecart(const entity & e) { return (entity_type_defs[e.type].flags & ENTITY_FLAG_MINECART) != 0; }

bool entity_is_boat(const entity & e) { return (entity_type_defs[e.type].flags & ENTITY_FLAG_BOAT) != 0; }

// Entity.isPushable: the vehicles
bool entity_is_pushable(const entity & e) { return entity_is_minecart(e) || entity_is_boat(e); }

bool entity_is_container(const entity & e) { return entity_type_defs[e.type].container_size > 0; }

container entity_container(entity & e) {
    container c;
    c.vehicle = &e;
    return c;
}

// Containers.dropContents at the entity
void container_entity_drop_contents(level & lv, entity & e) {
    containers_drop_contents(lv, e.pos, entity_container(e));
}

// destroy of the type: killed, then the vehicle drops as an item; a container drops its contents when killed, and
// ContainerEntity.chestVehicleDestroyed drops them again from the now empty slots
static void vehicle_entity_destroy(level & lv, entity & e, bool ignites_tnt) {
    if (e.type == ENTITY_TYPE_TNT_MINECART && minecart_tnt_destroy(e, ignites_tnt)) return;
    entity_kill(lv, e);
    item_entity_spawn(lv, e.pos, { entity_type_defs[e.type].vehicle_drop, 1, 0 });
    if (entity_is_container(e)) container_entity_drop_contents(lv, e);
}

// VehicleEntity.hurtServer: damage over 40 destroys the vehicle, and fire or an explosion destroys a tnt minecart.
// No vehicle is fire immune.
bool vehicle_entity_hurt_server(level & lv, entity & e, damage_type type, float damage) {
    if (e.removed) return true;
    e.hurt_time = 10;
    e.damage += damage * 10.0F;
    bool ignites_tnt = (damage_type_tags[type] & (DAMAGE_TYPE_TAG_IS_FIRE | DAMAGE_TYPE_TAG_IS_EXPLOSION)) != 0;
    if (e.damage > 40.0F || (e.type == ENTITY_TYPE_TNT_MINECART && ignites_tnt)) {
        vehicle_entity_destroy(lv, e, ignites_tnt);
    }
    return true;
}
