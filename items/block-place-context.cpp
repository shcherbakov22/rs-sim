// BlockPlaceContext: places in the clicked block if it can be replaced, else next to the clicked face

#include "items/block-place-context.h"

#include "blocks/block-behaviour.h"
#include "engine/level.h"

// replaceClicked is still true while the clicked block decides whether it can be replaced
block_place_context block_place_context_init(level & lv, const use_on_context & hit) {
    block_place_context ctx = { hit, pos_relative(hit.pos, hit.face), true };
    ctx.replace_clicked     = block_can_be_replaced(lv, level_get_state(lv, hit.pos), ctx);
    return ctx;
}

block_pos block_place_context_get_clicked_pos(const block_place_context & ctx) {
    return ctx.replace_clicked ? ctx.hit.pos : ctx.relative_pos;
}

bool block_place_context_can_place(level & lv, const block_place_context & ctx) {
    return ctx.replace_clicked ||
           block_can_be_replaced(lv, level_get_state(lv, block_place_context_get_clicked_pos(ctx)), ctx);
}

// placing next to a face looks into that face first
std::array<direction, 6> block_place_context_get_nearest_looking_directions(const block_place_context & ctx) {
    std::array<direction, 6> directions;
    std::copy(player_nearest_directions, player_nearest_directions + 6, directions.begin());
    if (ctx.replace_clicked) {
        return directions;
    }
    direction into  = direction_opposite(ctx.hit.face);
    size_t    index = 0;
    while (index < directions.size() && directions[index] != into) {
        index++;
    }
    if (index > 0) {
        std::copy_backward(directions.begin(), directions.begin() + index, directions.begin() + index + 1);
        directions[0] = into;
    }
    return directions;
}

// placing into a fluid is out of scope
bool block_place_context_is_water_source(level & lv, const block_place_context & ctx) {
    if (state_has(level_get_state(lv, block_place_context_get_clicked_pos(ctx)), STATE_FLAG_FLUID)) {
        level_unsupported(lv, "fluids");
    }
    return false;
}

double use_on_context_get_click_location(const use_on_context & hit, direction_axis axis) {
    const block_pos & step = direction_defs[hit.face].step;
    switch (axis) {
        case DIRECTION_AXIS_X:
            return hit.pos.x + 0.5 + step.x * 0.5;
        case DIRECTION_AXIS_Y:
            return hit.pos.y + 0.5 + step.y * 0.5;
        case DIRECTION_AXIS_Z:
            return hit.pos.z + 0.5 + step.z * 0.5;
    }
    TMC_ASSERT(false);
    return 0.0;
}
