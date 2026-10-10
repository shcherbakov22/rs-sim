#pragma once

// Handlers per vanilla item class, called from item-behaviour.cpp

#include "items/item-behaviour.h"

// BlockItem
interaction_result block_item_use_on(level & lv, const use_on_context & hit);
state_id           block_item_get_placement_state(level & lv, const block_place_context & ctx);
bool               block_item_place_block(level & lv, const block_place_context & ctx, state_id st);

// DoubleHighBlockItem
bool double_high_block_item_place_block(level & lv, const block_place_context & ctx, state_id st);

// HoneycombItem
interaction_result honeycomb_item_use_on(level & lv, const use_on_context & hit);

// AxeItem
interaction_result axe_item_use_on(level & lv, const use_on_context & hit);

// EnderEyeItem
interaction_result ender_eye_item_use_on(level & lv, const use_on_context & hit);

// ShearsItem
interaction_result shears_item_use_on(level & lv, const use_on_context & hit);

// ShovelItem
interaction_result shovel_item_use_on(level & lv, const use_on_context & hit);

// FireChargeItem
interaction_result fire_charge_item_use_on(level & lv, const use_on_context & hit);

// FlintAndSteelItem
interaction_result flint_and_steel_item_use_on(level & lv, const use_on_context & hit);

// StandingAndWallBlockItem
state_id standing_and_wall_block_item_get_placement_state(level & lv, const block_place_context & ctx);

// ScaffoldingBlockItem
bool scaffolding_block_item_update_placement_context(level & lv, block_place_context & ctx);
