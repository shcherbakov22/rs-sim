// BlockItem: places its block in the clicked block or next to it

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/handlers.h"

// BlockItem.canPlace; with no entities nothing obstructs a placement
static bool block_item_can_place(level & lv, const block_place_context & ctx, state_id st) {
    return !item_must_survive(lv, ctx) || block_can_survive(lv, st, block_place_context_get_clicked_pos(ctx));
}

state_id block_item_get_placement_state(level & lv, const block_place_context & ctx) {
    state_id st = block_get_state_for_placement(lv, item_defs[ctx.hit.stack.item].block, ctx);
    return st != STATE_NONE && block_item_can_place(lv, ctx, st) ? st : STATE_NONE;
}

bool block_item_place_block(level & lv, const block_place_context & ctx, state_id st) {
    return level_set_block(lv, block_place_context_get_clicked_pos(ctx), st, UPDATE_FLAG_ALL_IMMEDIATE);
}

// BlockItemStateProperties.apply skips names the block has no property for and values the property cannot take
static state_id block_item_state_properties_apply(std::string_view props, state_id st) {
    while (!props.empty()) {
        size_t           comma = props.find(',');
        std::string_view pair  = props.substr(0, comma);
        props                  = comma == std::string_view::npos ? std::string_view() : props.substr(comma + 1);
        size_t            eq   = pair.find('=');
        const block_def & b    = block_defs[state_block(st)];
        for (int32_t i = 0; i < b.n_props; i++) {
            const prop_def & p = prop_defs[block_props[b.props_first + i].prop];
            if (pair.substr(0, eq) != p.name) {
                continue;
            }
            for (int32_t v = 0; v < p.n_values; v++) {
                if (pair.substr(eq + 1) == p.values[v]) {
                    st = state_set(st, (prop_id) block_props[b.props_first + i].prop, p.codes[v]);
                }
            }
        }
    }
    return st;
}

static state_id block_item_update_block_state_from_tag(level & lv, block_pos pos, const char * props, state_id placed) {
    state_id modified = block_item_state_properties_apply(props, placed);
    if (modified != placed) {
        level_set_block(lv, pos, modified, UPDATE_FLAG_CLIENTS);
    }
    return modified;
}

// sounds, game events and the used up item are not in the trace
interaction_result block_item_place(level & lv, block_place_context ctx) {
    if (!block_place_context_can_place(lv, ctx) || !item_update_placement_context(lv, ctx)) {
        return INTERACTION_RESULT_FAIL;
    }
    state_id st = item_get_placement_state(lv, ctx);
    if (st == STATE_NONE || !item_place_block(lv, ctx, st)) {
        return INTERACTION_RESULT_FAIL;
    }
    block_pos pos    = block_place_context_get_clicked_pos(ctx);
    state_id  placed = level_get_state(lv, pos);
    if (state_is(placed, state_block(st))) {
        const char * block_state = item_defs[ctx.hit.stack.item].block_state;
        if (block_state != nullptr) {
            placed = block_item_update_block_state_from_tag(lv, pos, block_state, placed);
        }
        item_update_custom_block_entity_tag(lv, ctx);
        // applyComponentsFromItemStack changes nothing: a fresh stack has the defaults of a new block entity
        if (level_get_block_entity(lv, pos) != nullptr) {
            block_entity_set_changed(lv, pos, placed);
        }
        block_set_placed_by(lv, placed, pos);
    }
    return INTERACTION_RESULT_SUCCESS;
}

// a CONSUMABLE stack that fails to place falls back to Item.use, which only starts eating on the player
interaction_result block_item_use_on(level & lv, const use_on_context & hit) {
    return block_item_place(lv, block_place_context_init(lv, hit));
}
