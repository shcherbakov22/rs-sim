#pragma once

// Handlers per vanilla item class, called from item-behaviour.cpp

#include "items/item-behaviour.h"

// BlockItem
interaction_result block_item_use_on(level & lv, const use_on_context & hit);
state_id           block_item_get_placement_state(level & lv, const block_place_context & ctx);
bool               block_item_place_block(level & lv, const block_place_context & ctx, state_id st);

// DoubleHighBlockItem
bool double_high_block_item_place_block(level & lv, const block_place_context & ctx, state_id st);

// StandingAndWallBlockItem
state_id standing_and_wall_block_item_get_placement_state(level & lv, const block_place_context & ctx);
