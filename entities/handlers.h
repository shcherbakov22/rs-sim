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

// AbstractMinecart and its types
bool     entity_is_minecart(const entity & e);
// the Container entities: chest and hopper minecarts
bool     entity_is_container(const entity & e);
entity * abstract_minecart_create_minecart(level & lv, entity_type type, vec3 pos);
void     abstract_minecart_tick(level & lv, entity & e);
void     abstract_minecart_apply_effects_from_blocks(level & lv, entity & e);
// AbstractMinecartContainer.remove drops the contents
void     abstract_minecart_container_drop_contents(level & lv, entity & e);
bool     abstract_minecart_hurt_server(level & lv, entity & e, damage_type type, float damage);
void     minecart_tnt_cause_fall_damage(level & lv, entity & e, double fall_distance);
