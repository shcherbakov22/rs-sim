#pragma once

#include "engine/entity.h"

// ItemEntity; the constructor that draws a random motion from the level's random
entity * item_entity_new_random_motion(level & lv, vec3 pos, item_stack s);
// that ItemEntity with setDefaultPickUpDelay, added to the level
void     item_entity_spawn(level & lv, vec3 pos, item_stack s);
void     item_entity_set_item(level & lv, entity & e, item_stack s);
void     item_entity_tick(level & lv, entity & e);
bool     item_entity_fire_immune(const entity & e);
bool     item_entity_hurt_server(level & lv, entity & e, damage_type type, float damage);

// FallingBlockEntity
entity * falling_block_entity_fall(level & lv, block_pos pos, state_id st);
void     falling_block_entity_tick(level & lv, entity & e);
void     falling_block_entity_cause_fall_damage(entity & e, double fall_distance);
