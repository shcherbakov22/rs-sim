#pragma once

#include "blocks/block-behaviour.h"
#include "items/block-place-context.h"

// Item and BlockItem dispatch to the vanilla class of the item in hand. A hook the class overrides
// but the sim does not port yet stops the run as unsupported.
interaction_result item_use_on(level & lv, const use_on_context & hit);
bool               item_update_placement_context(level & lv, const block_place_context & ctx);
state_id           item_get_placement_state(level & lv, const block_place_context & ctx);  // STATE_NONE for null
bool               item_place_block(level & lv, const block_place_context & ctx, state_id st);
bool               item_must_survive(level & lv, const block_place_context & ctx);
void               item_update_custom_block_entity_tag(level & lv, const block_place_context & ctx);
bool               item_can_place(level & lv, const block_place_context & ctx, state_id st);
