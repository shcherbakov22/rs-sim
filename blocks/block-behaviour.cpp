#include "blocks/block-behaviour.h"

#include "core/trace.h"
#include "engine/level.h"

static int16_t hook_owner(state_id st, hook h) {
    return block_defs[state_block(st)].hooks[h];
}

static void hook_unsupported(level & lv, state_id st, hook h) {
    level_unsupported(lv, "%s.%s", vclass_names[hook_owner(st, h)], hook_names[h]);
}

state_id block_update_shape(level &   lv,
                            state_id  st,
                            block_pos pos,
                            direction dir,
                            block_pos neighbor_pos,
                            state_id  neighbor) {
    trace_enter(lv.tr, &pos, "shape %d %d %d %s %s %s", pos.x, pos.y, pos.z, state_to_string(st).c_str(),
                direction_defs[dir].name, state_to_string(neighbor).c_str());
    state_id result = st;
    switch (hook_owner(st, HOOK_UPDATE_SHAPE)) {
        case VCLASS_NONE:
            break;
        default:
            hook_unsupported(lv, st, HOOK_UPDATE_SHAPE);
    }
    (void) neighbor_pos;
    if (result != st && trace_wants(lv.tr, &pos)) {
        trace_add(lv.tr, "=> %s", state_to_string(result).c_str());
    }
    trace_exit(lv.tr);
    return result;
}

void block_handle_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block, bool moved_by_piston) {
    trace_enter(lv.tr, &pos, "nc %d %d %d %s by %s%s", pos.x, pos.y, pos.z, state_to_string(st).c_str(),
                block_defs[block].name, moved_by_piston ? " piston" : "");
    switch (hook_owner(st, HOOK_NEIGHBOR_CHANGED)) {
        case VCLASS_NONE:
            break;
        default:
            hook_unsupported(lv, st, HOOK_NEIGHBOR_CHANGED);
    }
    trace_exit(lv.tr);
}

void block_on_place(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston) {
    (void) pos;
    (void) old;
    (void) moved_by_piston;
    switch (hook_owner(st, HOOK_ON_PLACE)) {
        case VCLASS_NONE:
            break;
        default:
            hook_unsupported(lv, st, HOOK_ON_PLACE);
    }
}

void block_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    (void) pos;
    (void) moved_by_piston;
    switch (hook_owner(st, HOOK_AFFECT_NEIGHBORS_AFTER_REMOVAL)) {
        case VCLASS_NONE:
            break;
        default:
            hook_unsupported(lv, st, HOOK_AFFECT_NEIGHBORS_AFTER_REMOVAL);
    }
}

void block_tick(level & lv, state_id st, block_pos pos) {
    (void) pos;
    switch (hook_owner(st, HOOK_TICK)) {
        case VCLASS_NONE:
            break;
        default:
            hook_unsupported(lv, st, HOOK_TICK);
    }
}

bool block_trigger_event(level & lv, state_id st, block_pos pos, int32_t param_a, int32_t param_b) {
    (void) pos;
    (void) param_a;
    (void) param_b;
    switch (hook_owner(st, HOOK_TRIGGER_EVENT)) {
        case VCLASS_NONE:
            return false;
        default:
            hook_unsupported(lv, st, HOOK_TRIGGER_EVENT);
            return false;
    }
}

void block_update_indirect_neighbour_shapes(level & lv, state_id st, block_pos pos, int32_t flags, int32_t limit) {
    (void) pos;
    (void) flags;
    (void) limit;
    switch (hook_owner(st, HOOK_UPDATE_INDIRECT_NEIGHBOUR_SHAPES)) {
        case VCLASS_NONE:
            break;
        default:
            hook_unsupported(lv, st, HOOK_UPDATE_INDIRECT_NEIGHBOUR_SHAPES);
    }
}

void block_spawn_after_break(level & lv, state_id st, block_pos pos) {
    (void) pos;
    switch (hook_owner(st, HOOK_SPAWN_AFTER_BREAK)) {
        case VCLASS_NONE:
            break;
        default:
            hook_unsupported(lv, st, HOOK_SPAWN_AFTER_BREAK);
    }
}

void block_update_neighbour_shapes(level & lv, state_id st, block_pos pos, int32_t flags, int32_t limit) {
    for (direction dir : UPDATE_SHAPE_ORDER) {
        level_neighbor_shape_changed(lv, direction_opposite(dir), pos_relative(pos, dir), pos, st, flags, limit);
    }
}

void block_update_or_destroy(level & lv, state_id old, state_id updated, block_pos pos, int32_t flags, int32_t limit) {
    if (updated == old) {
        return;
    }
    if (state_has(updated, STATE_FLAG_AIR)) {
        level_destroy_block(lv, pos, (flags & UPDATE_FLAG_SUPPRESS_DROPS) == 0, limit);
    } else {
        level_set_block(lv, pos, updated, flags & ~UPDATE_FLAG_SUPPRESS_DROPS, limit);
    }
}
