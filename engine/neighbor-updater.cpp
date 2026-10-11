#include "engine/neighbor-updater.h"

#include "blocks/block-behaviour.h"
#include "engine/level.h"

// Level.maxChainedNeighborUpdates (game rule default)
static constexpr int32_t MAX_CHAINED_NEIGHBOR_UPDATES = 1000000;

// NeighborUpdater.executeShapeUpdate
static void execute_shape_update(level & lv, const neighbor_update & u) {
    state_id current = level_get_state(lv, u.pos);
    if ((u.flags & UPDATE_FLAG_SKIP_SHAPE_UPDATE_ON_WIRE) == 0 || !state_is(current, BLOCK_REDSTONE_WIRE)) {
        state_id updated = block_update_shape(lv, current, u.pos, u.dir, u.neighbor_pos, u.state);
        block_update_or_destroy(lv, current, updated, u.pos, u.flags, u.limit);
    }
}

// NeighborUpdates.runNext: true while the update has more to do
static bool run_next(level & lv, neighbor_update & u) {
    switch (u.type) {
        case NEIGHBOR_UPDATE_SHAPE: execute_shape_update(lv, u); return false;
        case NEIGHBOR_UPDATE_SIMPLE:
            block_handle_neighbor_changed(lv, level_get_state(lv, u.pos), u.pos, u.block, false);
            return false;
        case NEIGHBOR_UPDATE_FULL:
            block_handle_neighbor_changed(lv, u.state, u.pos, u.block, u.moved_by_piston);
            return false;
        case NEIGHBOR_UPDATE_MULTI:
            {
                block_pos neighbor = pos_relative(u.pos, NEIGHBOR_UPDATE_ORDER[u.idx++]);
                block_handle_neighbor_changed(lv, level_get_state(lv, neighbor), neighbor, u.block, false);
                if (u.idx < 6 && (int32_t) NEIGHBOR_UPDATE_ORDER[u.idx] == u.skip) u.idx++;
                return u.idx < 6;
            }
    }
    return false;
}

static void run_updates(level & lv) {
    neighbor_updater & nu = lv.updater;
    while (!nu.stack.empty() || !nu.added_this_layer.empty()) {
        // the first update added this layer ends up on top
        for (size_t i = nu.added_this_layer.size(); i-- > 0;) nu.stack.push_back(nu.added_this_layer[i]);
        nu.added_this_layer.clear();
        // updates queued meanwhile go to added_this_layer, so the top stays the same
        while (nu.added_this_layer.empty()) {
            if (!run_next(lv, nu.stack.back())) {
                nu.stack.pop_back();
                break;
            }
        }
    }
    nu.stack.clear();
    nu.added_this_layer.clear();
    nu.count = 0;
}

void neighbor_updater_add(level & lv, const neighbor_update & update) {
    neighbor_updater & nu       = lv.updater;
    bool               running  = nu.count > 0;
    bool               too_many = nu.count >= MAX_CHAINED_NEIGHBOR_UPDATES;
    nu.count++;
    // past the limit vanilla drops the update (and logs once)
    if (!too_many && running) {
        nu.added_this_layer.push_back(update);
    } else if (!too_many) {
        nu.stack.push_back(update);
    }
    if (!running) run_updates(lv);
}
