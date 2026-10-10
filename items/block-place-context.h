#pragma once

#include "core/direction.h"
#include "core/item-stack.h"

#include <array>

struct level;

// UseOnContext of the virtual player, who never sneaks; Inputs.use hits the middle of the clicked face
struct use_on_context {
    block_pos  pos;
    direction  face;
    item_stack stack;
};

// BlockPlaceContext
struct block_place_context {
    use_on_context hit;
    block_pos      relative_pos;
    bool           replace_clicked;
};

block_place_context      block_place_context_init(level & lv, const use_on_context & hit);
block_pos                block_place_context_get_clicked_pos(const block_place_context & ctx);
bool                     block_place_context_can_place(level & lv, const block_place_context & ctx);
std::array<direction, 6> block_place_context_get_nearest_looking_directions(const block_place_context & ctx);
// getFluidState(getClickedPos()).getType() == Fluids.WATER
bool                     block_place_context_is_water_source(level & lv, const block_place_context & ctx);
// UseOnContext.getClickLocation on one axis
double                   use_on_context_get_click_location(const use_on_context & hit, direction_axis axis);
