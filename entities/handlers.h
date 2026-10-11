#pragma once

#include "engine/entity.h"

// ItemEntity; the constructor that draws a random motion from the level's random
entity * item_entity_new_random_motion(level & lv, vec3 pos, item_stack s);
void     item_entity_set_item(level & lv, entity & e, item_stack s);
void     item_entity_tick(level & lv, entity & e);
bool     item_entity_fire_immune(const entity & e);
bool     item_entity_hurt_server(level & lv, entity & e, damage_type type, float damage);
