#pragma once

// Ported vanilla block classes, one section per sim/blocks/<class>.cpp. block-behaviour.cpp dispatches the
// hooks to them; the other functions are vanilla's non-hook methods that other classes call.

#include "blocks/block-behaviour.h"

// BaseTorchBlock
state_id base_torch_update_shape(level & lv, state_id st, block_pos pos, direction dir);
bool     base_torch_can_survive(level & lv, block_pos pos);

// WallTorchBlock
state_id wall_torch_update_shape(level & lv, state_id st, block_pos pos, direction dir);
bool     wall_torch_can_survive(level & lv, block_pos pos, direction facing);

// RedStoneWireBlock
state_id red_stone_wire_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor);
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
bool    diode_can_survive_on(level & lv, state_id neighbor);
void    diode_tick(level & lv, state_id st, block_pos pos);
int32_t diode_get_signal(state_id st, direction dir);
void    diode_neighbor_changed(level & lv, state_id st, block_pos pos);
int32_t diode_get_alternate_signal(level & lv, state_id st, block_pos pos);
void    diode_on_place(level & lv, state_id st, block_pos pos);
void    diode_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston);

// RepeaterBlock
interaction_result repeater_use_without_item(level & lv, state_id st, block_pos pos);
int32_t            repeater_get_delay(state_id st);
state_id           repeater_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor);
bool               repeater_is_locked(level & lv, state_id st, block_pos pos);

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
