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
bool     diode_can_survive(level & lv, block_pos pos);
bool     diode_can_survive_on(level & lv, block_pos neighbor_pos, state_id neighbor);
void     diode_tick(level & lv, state_id st, block_pos pos);
int32_t  diode_get_signal(level & lv, state_id st, block_pos pos, direction dir);
void     diode_neighbor_changed(level & lv, state_id st, block_pos pos);
int32_t  diode_get_input_signal(level & lv, state_id st, block_pos pos);
int32_t  diode_get_alternate_signal(level & lv, state_id st, block_pos pos);
bool     diode_should_prioritize(level & lv, state_id st, block_pos pos);
void     diode_update_neighbors_in_front(level & lv, state_id st, block_pos pos);
void     diode_on_place(level & lv, state_id st, block_pos pos);
void     diode_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston);

// RepeaterBlock
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
bool               comparator_should_turn_on(level & lv, state_id st, block_pos pos);
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
int32_t      container_get_max_stack_size_in(const container & c, const item_stack & s);
item_stack   container_remove_item(level & lv, const container & c, int32_t slot, int32_t count);
void         container_set_item(level & lv, const container & c, int32_t slot, item_stack s);
void         container_set_changed(level & lv, const container & c);
int32_t      container_get_redstone_signal(const container * c);
void         containers_drop_contents(level & lv, block_pos pos, const container & c);
int32_t      container_get_redstone_signal_from_block_entity(level & lv, block_pos pos);

// EnderChestBlock
void ender_chest_tick(level & lv, block_pos pos);

// AbstractSkullBlock
void abstract_skull_neighbor_changed(level & lv, state_id st, block_pos pos);

// SnowyDirtBlock
state_id snowy_dirt_update_shape(state_id st, direction dir, state_id neighbor);

// DirtPathBlock
bool     dirt_path_can_survive(level & lv, block_pos pos);
state_id dirt_path_update_shape(level & lv, state_id st, block_pos pos, direction dir);
void     dirt_path_tick(level & lv, block_pos pos);

// BarrelBlock
void barrel_tick(level & lv, block_pos pos);

// HopperBlock
void     hopper_on_place(level & lv, state_id st, block_pos pos, state_id old);
void     hopper_neighbor_changed(level & lv, state_id st, block_pos pos);

// HopperBlockEntity
void hopper_block_entity_push_items_tick(level & lv, block_pos pos, state_id st, block_entity * be);
bool hopper_block_entity_get_container_at(level & lv, block_pos pos, container & out, composter_container & composter);
item_stack hopper_block_entity_add_item(level &           lv,
                                        const container * from,
                                        const container & to,
                                        item_stack        s,
                                        const direction * face);

// DispenserBlock, DispenserBlockEntity, DefaultDispenseItemBehavior
direction  dispenser_facing(state_id st);
int32_t    dispenser_block_entity_get_random_slot(level & lv, const block_entity * be);
void       default_dispense_item_behavior_spawn_item(level & lv, block_pos pos, direction dir);
item_stack default_dispense_item_behavior_dispense(level & lv, state_id st, block_pos pos, item_stack dispensed);
item_stack default_dispense_item_behavior_consume_with_remainder(level &    lv,
                                                                 state_id   st,
                                                                 block_pos  pos,
                                                                 item_stack dispensed,
                                                                 item_stack remainder);
void       dispenser_tick(level & lv, state_id st, block_pos pos);
void       dispenser_neighbor_changed(level & lv, state_id st, block_pos pos);

// DispenseItemBehavior and its subclasses
item_stack dispense_item_behavior_dispense(level & lv, state_id st, block_pos pos, item_stack dispensed);
item_stack projectile_dispense_behavior_execute(level & lv, state_id st, block_pos pos, item_stack dispensed);
item_stack boat_dispense_item_behavior_execute(level & lv, state_id st, block_pos pos, item_stack dispensed);
item_stack minecart_dispense_item_behavior_execute(level & lv, state_id st, block_pos pos, item_stack dispensed);

// BellBlock
bool               bell_can_survive(level & lv, state_id st, block_pos pos);
state_id           bell_update_shape(level &   lv,
                                     state_id  st,
                                     block_pos pos,
                                     direction dir,
                                     block_pos neighbor_pos,
                                     state_id  neighbor);
void               bell_neighbor_changed(level & lv, state_id st, block_pos pos);

// CrafterBlock, CrafterBlockEntity
bool     crafter_block_entity_can_place_item(const block_entity * be, int32_t slot);
void     crafter_block_entity_server_tick(level & lv, block_pos pos, state_id st, block_entity * be);
int32_t  crafter_get_analog_output_signal(level & lv, block_pos pos);
void     crafter_neighbor_changed(level & lv, state_id st, block_pos pos);
void     crafter_tick(level & lv, state_id st, block_pos pos);

// JukeboxBlock, JukeboxBlockEntity, JukeboxSongPlayer
bool               jukebox_song_has_finished(int32_t record, int64_t ticks);
void               jukebox_block_entity_set_the_item(level & lv, block_entity * be, item_stack s);
void               jukebox_block_entity_pop_out_the_item(level & lv, block_entity * be);
void               jukebox_block_entity_tick(level & lv, block_entity * be);
int32_t            jukebox_get_signal(level & lv, block_pos pos);
int32_t            jukebox_get_analog_output_signal(level & lv, block_pos pos);

// ChiseledBookShelfBlock, ChiseledBookShelfBlockEntity
item_stack chiseled_book_shelf_block_entity_remove_item(level & lv, block_entity * be, int32_t slot);
void       chiseled_book_shelf_block_entity_set_item(level & lv, block_entity * be, int32_t slot, item_stack s);
bool       chiseled_book_shelf_block_entity_can_place_item(const block_entity * be, int32_t slot, const item_stack & s);
bool       chiseled_book_shelf_block_entity_can_take_item(const container & into, const item_stack & s);
int32_t    chiseled_book_shelf_get_analog_output_signal(level & lv, block_pos pos);

// ShelfBlock, ShelfBlockEntity, SideChainPartBlock
void               shelf_on_place(level & lv, state_id st, block_pos pos, state_id old);
void               shelf_neighbor_changed(level & lv, state_id st, block_pos pos);
void               shelf_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos);
int32_t            shelf_get_analog_output_signal(level & lv, state_id st, block_pos pos, direction dir);

// CrossCollisionBlock, FenceBlock
prop_id  cross_collision_property_by_direction(direction dir);
state_id fence_update_shape(level & lv, state_id st, direction dir, block_pos neighbor_pos, state_id neighbor);

// IronBarsBlock
state_id iron_bars_update_shape(level & lv, state_id st, direction dir, block_pos neighbor_pos, state_id neighbor);

// LeavesBlock
void     leaves_tick(level & lv, state_id st, block_pos pos);
state_id leaves_update_shape(level & lv, state_id st, block_pos pos, state_id neighbor);

// StairBlock
state_id stair_update_shape(level & lv, state_id st, block_pos pos, direction dir);

// TrapDoorBlock
void               trap_door_neighbor_changed(level & lv, state_id st, block_pos pos);

// WallBlock
state_id wall_update_shape(level &   lv,
                           state_id  st,
                           block_pos pos,
                           direction dir,
                           block_pos neighbor_pos,
                           state_id  neighbor);

// FenceGateBlock
state_id           fence_gate_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor);
void               fence_gate_neighbor_changed(level & lv, state_id st, block_pos pos);

// BaseRailBlock and RailState of the straight rails
struct rail_state {
    block_pos              pos;
    state_id               state;
    std::vector<block_pos> connections;
};

bool       base_rail_is_rail(level & lv, state_id st);
bool       base_rail_is_slope(int32_t shape);
state_id   base_rail_update_dir(level & lv, block_pos pos, state_id st, bool first);
state_id   base_rail_update_state(level & lv, state_id st, block_pos pos, bool moved_by_piston);
void       base_rail_on_place(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston);
void       base_rail_neighbor_changed(level & lv, state_id st, block_pos pos, bool moved_by_piston);
void       base_rail_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston);
rail_state rail_state_init(state_id st, block_pos pos);
void       rail_state_place(level & lv, rail_state & rs, bool first, int32_t default_shape);

// PoweredRailBlock
void powered_rail_update_state(level & lv, state_id st, block_pos pos);

// CarpetBlock
state_id carpet_update_shape(level & lv, state_id st, block_pos pos);
bool     carpet_can_survive(level & lv, block_pos pos);

// LadderBlock
bool     ladder_can_survive(level & lv, state_id st, block_pos pos);
state_id ladder_update_shape(level & lv, state_id st, block_pos pos, direction dir);

// LanternBlock
bool     lantern_can_survive(level & lv, state_id st, block_pos pos);
state_id lantern_update_shape(level & lv, state_id st, block_pos pos, direction dir);

// LightningRodBlock
void    lightning_rod_on_place(level & lv, state_id st, block_pos pos, state_id old);
void    lightning_rod_tick(level & lv, state_id st, block_pos pos);
void    lightning_rod_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos);
int32_t lightning_rod_get_signal(state_id st);
int32_t lightning_rod_get_direct_signal(state_id st, direction dir);

// CopperBulbBlock
void    copper_bulb_check_and_flip(level & lv, state_id st, block_pos pos);
void    copper_bulb_on_place(level & lv, state_id st, block_pos pos, state_id old);
int32_t copper_bulb_get_analog_output_signal(level & lv, block_pos pos);

// NoteBlock
state_id           note_block_update_shape(level & lv, state_id st, block_pos pos, direction dir);
void               note_block_neighbor_changed(level & lv, state_id st, block_pos pos);
bool               note_block_trigger_event(level & lv, state_id st);

// TargetBlock
void    target_tick(level & lv, state_id st, block_pos pos);
int32_t target_get_signal(state_id st);
void    target_on_place(level & lv, state_id st, block_pos pos, state_id old);

// SoulSandBlock
void soul_sand_on_place(level & lv, state_id st, block_pos pos);
void soul_sand_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor);
void soul_sand_tick(level & lv, block_pos pos);

// ScaffoldingBlock
int32_t  scaffolding_get_distance(level & lv, block_pos pos);
void     scaffolding_on_place(level & lv, state_id st, block_pos pos);
void     scaffolding_update_shape(level & lv, state_id st, block_pos pos);
void     scaffolding_tick(level & lv, state_id st, block_pos pos);

// ComposterBlock
void               composter_on_place(level & lv, state_id st, block_pos pos);
void               composter_tick(level & lv, state_id st, block_pos pos);
int32_t            composter_get_analog_output_signal(state_id st);
void               composter_get_container(state_id st, block_pos pos, composter_container & out);
int32_t            composter_container_get_container_size(const composter_container & c);
int32_t            composter_container_get_slots_for_face(const composter_container & c, direction face);
bool               composter_container_can_place_item_through_face(const composter_container & c,
                                                                   const item_stack &          s,
                                                                   const direction *           face);
bool               composter_container_can_take_item_through_face(const composter_container & c,
                                                                  const item_stack &          s,
                                                                  direction                   face);
void               composter_container_set_changed(level & lv, composter_container & c);

// DoorBlock
bool               door_can_survive(level & lv, state_id st, block_pos pos);
state_id           door_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor);
void               door_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block);

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
void               lever_pull(level & lv, state_id st, block_pos pos);
void               lever_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston);
int32_t            lever_get_signal(state_id st);
int32_t            lever_get_direct_signal(state_id st, direction dir);

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
voxel_shape        moving_piston_get_collision_shape(level & lv, block_pos pos);

// PistonBaseBlock
void     piston_base_neighbor_changed(level & lv, state_id st, block_pos pos);
void     piston_base_on_place(level & lv, state_id st, block_pos pos, state_id old);
bool     piston_base_trigger_event(level & lv, state_id st, block_pos pos, int32_t b0, int32_t b1);
bool     piston_base_is_pushable(state_id  st,
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

// BaseFireBlock
state_id base_fire_get_state(level & lv, block_pos pos);
void     base_fire_on_place(level & lv, state_id st, block_pos pos, state_id old);
bool     base_fire_can_be_placed_at(level & lv, block_pos pos, direction forward);

// FireBlock
state_id fire_get_state_for_placement(level & lv, block_pos pos);
state_id fire_update_shape(level & lv, state_id st, block_pos pos);
bool     fire_can_survive(level & lv, block_pos pos);
void     fire_on_place(level & lv, state_id st, block_pos pos, state_id old);
void     fire_tick(level & lv, block_pos pos);

// SoulFireBlock
state_id soul_fire_update_shape(level & lv, state_id st, block_pos pos);
bool     soul_fire_can_survive(level & lv, block_pos pos);
bool     soul_fire_can_survive_on_block(state_id st);

// TntBlock
void               tnt_prime(level & lv, block_pos pos);
void               tnt_on_place(level & lv, state_id st, block_pos pos, state_id old);
void               tnt_neighbor_changed(level & lv, block_pos pos);

// FallingBlock, ConcretePowderBlock
void     falling_block_on_place(level & lv, state_id st, block_pos pos);
state_id falling_block_update_shape(level & lv, state_id st, block_pos pos);
void     falling_block_tick(level & lv, block_pos pos);
state_id concrete_powder_update_shape(level & lv, state_id st, block_pos pos);

// PortalShape
struct portal_shape {
    direction_axis axis;
    int32_t        num_portal_blocks;
    direction      right_dir;
    block_pos      bottom_left;
    int32_t        height;
    int32_t        width;
};

portal_shape portal_shape_find_any_shape(level & lv, block_pos pos, direction_axis axis);
bool portal_shape_find_empty_portal_shape(level & lv, block_pos pos, direction_axis preferred_axis, portal_shape & out);
bool portal_shape_is_complete(const portal_shape & s);
void portal_shape_create_portal_blocks(level & lv, const portal_shape & s);

// NetherPortalBlock
state_id nether_portal_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor);
