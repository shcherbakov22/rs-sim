#pragma once

// Ported vanilla block classes, one section per sim/blocks/<class>.cpp. block-behaviour.cpp dispatches the
// hooks to them; the other functions are vanilla's non-hook methods that other classes call.

#include "blocks/block-behaviour.h"
#include "core/voxel-shape.h"
#include "engine/block-entity.h"

#include <vector>

// AmethystClusterBlock
state_id amethyst_cluster_update_shape(level & lv, state_id st, block_pos pos, direction dir);
bool     amethyst_cluster_can_survive(level & lv, state_id st, block_pos pos);

// BaseTorchBlock
state_id base_torch_update_shape(level & lv, state_id st, block_pos pos, direction dir);
bool     base_torch_can_survive(level & lv, block_pos pos);

// WallTorchBlock
state_id wall_torch_update_shape(level & lv, state_id st, block_pos pos, direction dir);
bool     wall_torch_can_survive(level & lv, block_pos pos, direction facing);

// RedStoneWireBlock
state_id red_stone_wire_update_shape(level &   lv,
                                     state_id  st,
                                     block_pos pos,
                                     direction dir,
                                     block_pos neighbor_pos,
                                     state_id  neighbor);
void     red_stone_wire_update_indirect_neighbour_shapes(level &   lv,
                                                         state_id  st,
                                                         block_pos pos,
                                                         int32_t   flags,
                                                         int32_t   limit);
bool     red_stone_wire_can_survive(level & lv, block_pos pos);
void     red_stone_wire_on_place(level & lv, state_id st, block_pos pos, state_id old);
void     red_stone_wire_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston);
void     red_stone_wire_neighbor_changed(level & lv, state_id st, block_pos pos);
int32_t  red_stone_wire_get_direct_signal(level & lv, state_id st, block_pos pos, direction dir);
int32_t  red_stone_wire_get_signal(level & lv, state_id st, block_pos pos, direction dir);
interaction_result red_stone_wire_use_without_item(level & lv, state_id st, block_pos pos);

// RedstoneTorchBlock
void    redstone_torch_on_place(level & lv, state_id st, block_pos pos);
void    redstone_torch_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston);
int32_t redstone_torch_get_signal(state_id st, direction dir);
void    redstone_torch_tick(level & lv, state_id st, block_pos pos);
void    redstone_torch_neighbor_changed(level & lv, state_id st, block_pos pos);
int32_t redstone_torch_get_direct_signal(level & lv, state_id st, block_pos pos, direction dir);

// RedstoneWallTorchBlock
bool     redstone_wall_torch_can_survive(level & lv, state_id st, block_pos pos);
state_id redstone_wall_torch_update_shape(level & lv, state_id st, block_pos pos, direction dir);
bool     redstone_wall_torch_has_neighbor_signal(level & lv, state_id st, block_pos pos);
int32_t  redstone_wall_torch_get_signal(state_id st, direction dir);

// DiodeBlock
bool    diode_can_survive(level & lv, block_pos pos);
bool    diode_can_survive_on(level & lv, block_pos neighbor_pos, state_id neighbor);
void    diode_tick(level & lv, state_id st, block_pos pos);
int32_t diode_get_signal(level & lv, state_id st, block_pos pos, direction dir);
void    diode_neighbor_changed(level & lv, state_id st, block_pos pos);
int32_t diode_get_input_signal(level & lv, state_id st, block_pos pos);
int32_t diode_get_alternate_signal(level & lv, state_id st, block_pos pos);
bool    diode_should_prioritize(level & lv, state_id st, block_pos pos);
void    diode_update_neighbors_in_front(level & lv, state_id st, block_pos pos);
void    diode_on_place(level & lv, state_id st, block_pos pos);
void    diode_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston);

// RepeaterBlock
interaction_result repeater_use_without_item(level & lv, state_id st, block_pos pos);
int32_t            repeater_get_delay(state_id st);
state_id           repeater_update_shape(level &   lv,
                                         state_id  st,
                                         block_pos pos,
                                         direction dir,
                                         block_pos neighbor_pos,
                                         state_id  neighbor);
bool               repeater_is_locked(level & lv, state_id st, block_pos pos);

// ComparatorBlock
state_id comparator_update_shape(level & lv, state_id st, direction dir, block_pos neighbor_pos, state_id neighbor);
int32_t  comparator_get_output_signal(level & lv, block_pos pos);
bool     comparator_should_turn_on(level & lv, state_id st, block_pos pos);
interaction_result comparator_use_without_item(level & lv, state_id st, block_pos pos);
void               comparator_check_tick_on_neighbor(level & lv, state_id st, block_pos pos);
void               comparator_tick(level & lv, state_id st, block_pos pos);

// BaseEntityBlock
bool base_entity_block_trigger_event(level & lv, block_pos pos, int32_t param_a, int32_t param_b);

// Container
void         block_entity_set_changed(level & lv, block_pos pos, state_id st);
bool         block_entity_is_container(const block_entity * be);
int32_t      container_get_container_size(const container & c);
item_stack & container_get_item(const container & c, int32_t slot);
bool         container_is_empty(const container & c);
int32_t      container_get_max_stack_size(const item_stack & s);
item_stack   container_remove_item(level & lv, const container & c, int32_t slot, int32_t count);
void         container_set_item(level & lv, const container & c, int32_t slot, item_stack s);
void         container_set_changed(level & lv, const container & c);
int32_t      container_get_redstone_signal(const container * c);
void         containers_drop_contents(level & lv, block_pos pos, const container & c);
int32_t      container_get_redstone_signal_from_block_entity(level & lv, block_pos pos);

// EnderChestBlock
void               ender_chest_tick(level & lv, block_pos pos);
interaction_result ender_chest_use_without_item(level & lv, block_pos pos);

// AbstractSkullBlock
void abstract_skull_neighbor_changed(level & lv, state_id st, block_pos pos);

// BarrelBlock
void barrel_tick(level & lv, block_pos pos);

// HopperBlock
void hopper_on_place(level & lv, state_id st, block_pos pos, state_id old);
void hopper_neighbor_changed(level & lv, state_id st, block_pos pos);

// HopperBlockEntity
void       hopper_block_entity_push_items_tick(level & lv, block_pos pos, state_id st, block_entity * be);
bool       hopper_block_entity_get_container_at(level & lv, block_pos pos, container & out);
item_stack hopper_block_entity_add_item(level & lv, const container * from, const container & to, item_stack s);

// DispenserBlock, DispenserBlockEntity, DefaultDispenseItemBehavior
int32_t    dispenser_block_entity_get_random_slot(level & lv, const block_entity * be);
item_stack default_dispense_item_behavior_dispense(level & lv, state_id st, block_pos pos, item_stack dispensed);
void       dispenser_tick(level & lv, state_id st, block_pos pos);
void       dispenser_neighbor_changed(level & lv, state_id st, block_pos pos);

// BellBlock
bool               bell_can_survive(level & lv, state_id st, block_pos pos);
state_id           bell_update_shape(level &   lv,
                                     state_id  st,
                                     block_pos pos,
                                     direction dir,
                                     block_pos neighbor_pos,
                                     state_id  neighbor);
void               bell_neighbor_changed(level & lv, state_id st, block_pos pos);
interaction_result bell_use_without_item(level & lv, state_id st, block_pos pos, direction face);

// LecternBlock, LecternBlockEntity
void               lectern_tick(level & lv, state_id st, block_pos pos);
void               lectern_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos);
int32_t            lectern_get_signal(state_id st);
int32_t            lectern_get_direct_signal(state_id st, direction dir);
int32_t            lectern_get_analog_output_signal(level & lv, state_id st, block_pos pos);
interaction_result lectern_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack);
interaction_result lectern_use_without_item(level & lv, state_id st);
void               lectern_block_entity_pre_remove_side_effects(level & lv, block_pos pos, state_id st);

// CrafterBlock, CrafterBlockEntity
bool    crafter_block_entity_can_place_item(const block_entity * be, int32_t slot);
void    crafter_block_entity_server_tick(level & lv, block_pos pos, state_id st, block_entity * be);
int32_t crafter_get_analog_output_signal(level & lv, block_pos pos);
void    crafter_neighbor_changed(level & lv, state_id st, block_pos pos);
void    crafter_tick(level & lv, block_pos pos);

// DecoratedPotBlock
interaction_result decorated_pot_use_item_on(level & lv, block_pos pos, const item_stack & stack);
interaction_result decorated_pot_use_without_item(level & lv, block_pos pos);

// JukeboxBlock, JukeboxBlockEntity, JukeboxSongPlayer
bool               jukebox_song_has_finished(int32_t record, int64_t ticks);
void               jukebox_block_entity_set_the_item(level & lv, block_entity * be, item_stack s);
void               jukebox_block_entity_pop_out_the_item(level & lv, block_entity * be);
void               jukebox_block_entity_tick(level & lv, block_entity * be);
int32_t            jukebox_get_signal(level & lv, block_pos pos);
int32_t            jukebox_get_analog_output_signal(level & lv, block_pos pos);
interaction_result jukebox_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack);
interaction_result jukebox_use_without_item(level & lv, state_id st, block_pos pos);

// ChiseledBookShelfBlock, ChiseledBookShelfBlockEntity
item_stack chiseled_book_shelf_block_entity_remove_item(level & lv, block_entity * be, int32_t slot);
void       chiseled_book_shelf_block_entity_set_item(level & lv, block_entity * be, int32_t slot, item_stack s);
bool       chiseled_book_shelf_block_entity_can_place_item(const block_entity * be, int32_t slot, const item_stack & s);
bool       chiseled_book_shelf_block_entity_can_take_item(const container & into, const item_stack & s);
int32_t    chiseled_book_shelf_get_analog_output_signal(level & lv, block_pos pos);
interaction_result chiseled_book_shelf_use_item_on(level &            lv,
                                                   state_id           st,
                                                   block_pos          pos,
                                                   direction          face,
                                                   const item_stack & stack);
interaction_result chiseled_book_shelf_use_without_item(level & lv, state_id st, block_pos pos, direction face);

// ShelfBlock, ShelfBlockEntity, SideChainPartBlock
void               shelf_on_place(level & lv, state_id st, block_pos pos, state_id old);
void               shelf_neighbor_changed(level & lv, state_id st, block_pos pos);
void               shelf_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos);
int32_t            shelf_get_analog_output_signal(level & lv, state_id st, block_pos pos, direction dir);
interaction_result shelf_use_item_on(level & lv, state_id st, block_pos pos, direction face, const item_stack & stack);

// BeehiveBlock
int32_t            beehive_get_analog_output_signal(state_id st);
interaction_result beehive_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack);

// CopperGolemStatueBlock, WeatheringCopperGolemStatueBlock
int32_t            copper_golem_statue_get_analog_output_signal(state_id st);
interaction_result copper_golem_statue_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack);
interaction_result weathering_copper_golem_statue_use_item_on(level &            lv,
                                                              state_id           st,
                                                              block_pos          pos,
                                                              const item_stack & stack);

// RespawnAnchorBlock
int32_t            respawn_anchor_get_analog_output_signal(state_id st);
interaction_result respawn_anchor_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack);
interaction_result respawn_anchor_use_without_item(level & lv, state_id st, block_pos pos);

// IronBarsBlock
state_id iron_bars_update_shape(level & lv, state_id st, direction dir, block_pos neighbor_pos, state_id neighbor);

// DropperBlock
void dropper_dispense_from(level & lv, state_id st, block_pos pos);

// ChestBlock
bool     chest_get_container(level & lv, state_id st, block_pos pos, bool ignore_being_blocked, container & out);
state_id chest_update_shape(level & lv, state_id st, direction dir, state_id neighbor);
state_id copper_chest_update_shape(level & lv, state_id st, direction dir, state_id neighbor);
int32_t  chest_get_analog_output_signal(level & lv, state_id st, block_pos pos);
void     chest_tick(level & lv, block_pos pos);

// ChestBlockEntity
void    chest_block_entity_recheck_open(level & lv, block_entity * be);
int32_t chest_block_entity_get_open_count(level & lv, state_id st, block_pos pos);

// TrappedChestBlock
int32_t trapped_chest_get_signal(level & lv, state_id st, block_pos pos);
int32_t trapped_chest_get_direct_signal(level & lv, state_id st, block_pos pos, direction dir);

// FaceAttachedHorizontalDirectionalBlock
bool      face_attached_horizontal_directional_can_survive(level & lv, state_id st, block_pos pos);
state_id  face_attached_horizontal_directional_update_shape(level & lv, state_id st, block_pos pos, direction dir);
direction face_attached_horizontal_directional_get_connected_direction(state_id st);

// LeverBlock
interaction_result lever_use_without_item(level & lv, state_id st, block_pos pos);
void               lever_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston);
int32_t            lever_get_signal(state_id st);
int32_t            lever_get_direct_signal(state_id st, direction dir);

// ButtonBlock
interaction_result button_use_without_item(level & lv, state_id st, block_pos pos);
void               button_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston);
int32_t            button_get_signal(state_id st);
int32_t            button_get_direct_signal(state_id st, direction dir);
void               button_tick(level & lv, state_id st, block_pos pos);

// RedstoneLampBlock
void redstone_lamp_neighbor_changed(level & lv, state_id st, block_pos pos);
void redstone_lamp_tick(level & lv, state_id st, block_pos pos);

// ObserverBlock
void     observer_tick(level & lv, state_id st, block_pos pos);
state_id observer_update_shape(level & lv, state_id st, block_pos pos, direction dir);
int32_t  observer_get_signal(state_id st, direction dir);
void     observer_on_place(level & lv, state_id st, block_pos pos, state_id old);
void     observer_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos);

// MovingPistonBlock
interaction_result moving_piston_use_without_item(level & lv, block_pos pos);
voxel_shape        moving_piston_get_collision_shape(level & lv, block_pos pos);

// PistonBaseBlock
void piston_base_neighbor_changed(level & lv, state_id st, block_pos pos);
void piston_base_on_place(level & lv, state_id st, block_pos pos, state_id old);
bool piston_base_trigger_event(level & lv, state_id st, block_pos pos, int32_t b0, int32_t b1);
bool piston_base_is_pushable(state_id  st,
                             block_pos pos,
                             direction dir,
                             bool      allow_destroyable,
                             direction connection_direction);

// PistonHeadBlock
void     piston_head_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos);
state_id piston_head_update_shape(level & lv, state_id st, block_pos pos, direction dir);
bool     piston_head_can_survive(level & lv, state_id st, block_pos pos);
void     piston_head_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block);

// PistonMovingBlockEntity
void        piston_moving_block_entity_final_tick(level & lv, block_entity * be);
void        piston_moving_block_entity_tick(level & lv, block_entity * be);
voxel_shape piston_moving_block_entity_get_collision_shape(level & lv, const block_entity * be);

// PistonStructureResolver
struct piston_structure_resolver {
    block_pos              piston_pos;
    direction              piston_direction;
    bool                   extending;
    direction              push_direction;
    block_pos              start_pos;
    std::vector<block_pos> to_push;
    std::vector<block_pos> to_destroy;
};

piston_structure_resolver piston_structure_resolver_init(block_pos piston_pos, direction dir, bool extending);
bool                      piston_structure_resolver_resolve(level & lv, piston_structure_resolver & r);
