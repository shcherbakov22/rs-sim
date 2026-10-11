#pragma once

#include "engine/block-entity.h"
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

// VehicleEntity and ContainerEntity
bool      entity_is_minecart(const entity & e);
bool      entity_is_boat(const entity & e);
bool      entity_is_pushable(const entity & e);
// the Container entities: chest and hopper minecarts and chest boats
bool      entity_is_container(const entity & e);
container entity_container(entity & e);
// the remove of a container entity drops the contents
void      container_entity_drop_contents(level & lv, entity & e);
bool      vehicle_entity_hurt_server(level & lv, entity & e, damage_type type, float damage);

// AbstractMinecart and its types
void abstract_minecart_tick(level & lv, entity & e);
void abstract_minecart_apply_effects_from_blocks(level & lv, entity & e);
void minecart_tnt_cause_fall_damage(level & lv, entity & e, double fall_distance);
// MinecartTNT.destroy: true when the tnt minecart primes instead of breaking
bool minecart_tnt_destroy(entity & e, bool ignites_tnt);

// PrimedTnt
entity * primed_tnt_new(level & lv, vec3 pos);
void     primed_tnt_tick(level & lv, entity & e);

// AbstractBoat and its types
void abstract_boat_tick(level & lv, entity & e);
void abstract_boat_check_fall_damage(level & lv, entity & e, double ya, bool on_ground);
void abstract_boat_on_above_bubble_column(entity & e, bool drag_down);
