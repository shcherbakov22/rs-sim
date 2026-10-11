#pragma once

#include "blocks/block-behaviour.h"
#include "engine/block-entity.h"

#include <vector>

// hook tables, one per family file
extern const std::vector<block_hooks> dispenser_hooks;
extern const std::vector<block_hooks> hopper_hooks;
extern const std::vector<block_hooks> container_hooks;
extern const std::vector<block_hooks> piston_hooks;
extern const std::vector<block_hooks> diode_hooks;
extern const std::vector<block_hooks> redstone_hooks;
extern const std::vector<block_hooks> rail_hooks;
extern const std::vector<block_hooks> terrain_hooks;
extern const std::vector<block_hooks> fire_hooks;
extern const std::vector<block_hooks> powered_hooks;
extern const std::vector<block_hooks> connecting_hooks;
extern const std::vector<block_hooks> attached_hooks;
extern const std::vector<block_hooks> fluid_hooks;
extern const std::vector<block_hooks> landing_hooks;

// BaseEntityBlock
bool base_entity_block_trigger_event(level & lv, state_id st, block_pos pos, int32_t param_a, int32_t param_b);

// Container
void         block_entity_set_changed(level & lv, block_pos pos, state_id st);
bool         block_entity_is_container(const block_entity * be);
int32_t      container_get_container_size(const container & c);
item_stack & container_get_item(const container & c, int32_t slot);
bool         container_is_empty(const container & c);
int32_t      container_get_max_stack_size(const item_stack & s);
int32_t      container_get_max_stack_size_in(const container & c, const item_stack & s);
item_stack   container_remove_item(level & lv, const container & c, int32_t slot, int32_t count);
void         container_set_item(level & lv, const container & c, int32_t slot, item_stack s);
void         container_set_changed(level & lv, const container & c);
int32_t      container_get_redstone_signal(const container * c);
void         containers_drop_contents(level & lv, const vec3 & at, const container & c);
// the getAnalogOutputSignal and affectNeighborsAfterRemoval of most containers
int32_t      container_get_redstone_signal_from_block_entity(level & lv, state_id st, block_pos pos, direction dir);
void         containers_update_neighbours_after_destroy(level & lv, state_id st, block_pos pos, bool moved_by_piston);

// HopperBlockEntity
void hopper_block_entity_push_items_tick(level & lv, block_pos pos, state_id st, block_entity * be);
bool hopper_block_entity_get_container_at(level & lv, block_pos pos, container & out, composter_container & composter);
// HopperBlockEntity.suckInItems for a Hopper at the point at (getLevelX, getLevelY, getLevelZ)
bool       hopper_block_entity_suck_in_items(level & lv, const container & hopper, const vec3 & at, bool grid_aligned);
bool       hopper_block_entity_add_item_entity(level & lv, const container & to, entity & e);
item_stack hopper_block_entity_add_item(level & lv, const container * from, const container & to, item_stack s,
                                        const direction * face);

// CrafterBlock, CrafterBlockEntity
bool crafter_block_entity_can_place_item(const block_entity * be, int32_t slot);
void crafter_block_entity_server_tick(level & lv, block_pos pos, state_id st, block_entity * be);

// JukeboxBlock, JukeboxBlockEntity, JukeboxSongPlayer
bool jukebox_song_has_finished(int32_t record, int64_t ticks);
void jukebox_block_entity_set_the_item(level & lv, block_entity * be, item_stack s);
void jukebox_block_entity_pop_out_the_item(level & lv, block_entity * be);
void jukebox_block_entity_tick(level & lv, block_entity * be);

// ChiseledBookShelfBlock, ChiseledBookShelfBlockEntity
item_stack chiseled_book_shelf_block_entity_remove_item(level & lv, block_entity * be, int32_t slot);
void       chiseled_book_shelf_block_entity_set_item(level & lv, block_entity * be, int32_t slot, item_stack s);
bool       chiseled_book_shelf_block_entity_can_place_item(const block_entity * be, int32_t slot, const item_stack & s);
bool       chiseled_book_shelf_block_entity_can_take_item(const container & into, const item_stack & s);

// Fluid.getTickDelay
int32_t fluid_get_tick_delay(int32_t fluid);
// FluidState.tick (FlowingFluid.tick)
void    flowing_fluid_tick(level & lv, block_pos pos, state_id st, fluid_state_id fs);
// BubbleColumnBlock.updateColumn
void    bubble_column_update_column(level & lv, block_pos origin, state_id origin_state, state_id below);
// FluidState.getHeight and FlowingFluid.getFlow, for entities in fluids
float   fluid_state_get_height(level & lv, fluid_state_id fs, block_pos pos);
vec3    flowing_fluid_get_flow(level & lv, block_pos pos, fluid_state_id fs);

// BaseRailBlock
bool    base_rail_is_rail(state_id st);
bool    base_rail_is_slope(int32_t shape);
int32_t base_rail_get_shape(state_id st);

// ComposterBlock
void    composter_get_container(state_id st, block_pos pos, composter_container & out);
int32_t composter_container_get_container_size(const composter_container & c);
int32_t composter_container_get_slots_for_face(const composter_container & c, direction face);
bool    composter_container_can_place_item_through_face(const composter_container & c, const item_stack & s,
                                                        const direction * face);
bool    composter_container_can_take_item_through_face(const composter_container & c, const item_stack & s,
                                                       direction face);
void    composter_container_set_changed(level & lv, composter_container & c);

// ChestBlock
bool chest_get_container(level & lv, state_id st, block_pos pos, bool ignore_being_blocked, container & out);

direction face_attached_horizontal_directional_get_connected_direction(state_id st);
// the updateShape and canSurvive of a block on a sturdy face, which redstone wall torches share
state_id  attached_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos, state_id);
bool      attached_can_survive(level & lv, state_id st, block_pos pos);

// LeverBlock
void lever_pull(level & lv, state_id st, block_pos pos);

// MovingPistonBlock
shape moving_piston_get_collision_shape(level & lv, block_pos pos);
// MovingPistonBlock.getCollisionShape for an entity, as dumped shapes at world offsets; true when it is Shapes.block()
bool  moving_piston_get_entity_collision_shapes(level & lv, block_pos pos, std::vector<voxel_shape> & out);

// PistonMovingBlockEntity
void piston_moving_block_entity_final_tick(level & lv, block_entity * be);
void piston_moving_block_entity_tick(level & lv, block_entity * be);

// BaseFireBlock
state_id base_fire_get_state(level & lv, block_pos pos);
bool     base_fire_can_be_placed_at(level & lv, block_pos pos, direction forward);

// TntBlock
void tnt_prime(level & lv, block_pos pos);
