// RedstoneTorchBlock

#include "blocks/handlers.h"
#include "engine/level.h"

// RedstoneTorchBlock.RECENT_TOGGLE_TIMER, MAX_RECENT_TOGGLES, RESTART_DELAY, TOGGLE_DELAY
static constexpr int64_t REDSTONE_TORCH_RECENT_TOGGLE_TIMER = 60;
static constexpr int32_t REDSTONE_TORCH_MAX_RECENT_TOGGLES  = 8;
static constexpr int32_t REDSTONE_TORCH_RESTART_DELAY       = 160;
static constexpr int32_t REDSTONE_TORCH_TOGGLE_DELAY        = 2;

static void redstone_torch_notify_neighbors(level & lv, state_id st, block_pos pos) {
    for (int32_t d = 0; d < 6; d++) {
        level_update_neighbors_at(lv, pos_relative(pos, (direction) d), state_block(st));
    }
}

void redstone_torch_on_place(level & lv, state_id st, block_pos pos) {
    redstone_torch_notify_neighbors(lv, st, pos);
}

void redstone_torch_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (!moved_by_piston) {
        redstone_torch_notify_neighbors(lv, st, pos);
    }
}

int32_t redstone_torch_get_signal(state_id st, direction dir) {
    return state_get(st, PROP_LIT) && dir != DIRECTION_UP ? 15 : 0;
}

// virtual: RedstoneWallTorchBlock overrides it
static bool redstone_torch_has_neighbor_signal(level & lv, state_id st, block_pos pos) {
    if (block_instanceof(state_block(st), VCLASS_REDSTONE_WALL_TORCH_BLOCK)) {
        return redstone_wall_torch_has_neighbor_signal(lv, st, pos);
    }
    return level_has_signal(lv, pos_relative(pos, DIRECTION_DOWN), DIRECTION_DOWN);
}

static bool redstone_torch_is_toggled_too_frequently(level & lv, block_pos pos, bool add) {
    std::vector<redstone_torch_toggle> & toggles = lv.redstone_torch_recent_toggles;
    if (add) {
        toggles.push_back({ pos, lv.game_time });
    }
    int32_t count = 0;
    for (const redstone_torch_toggle & toggle : toggles) {
        if (toggle.pos == pos && ++count >= REDSTONE_TORCH_MAX_RECENT_TOGGLES) {
            return true;
        }
    }
    return false;
}

void redstone_torch_tick(level & lv, state_id st, block_pos pos) {
    bool                                 neighbor_signal = redstone_torch_has_neighbor_signal(lv, st, pos);
    std::vector<redstone_torch_toggle> & toggles         = lv.redstone_torch_recent_toggles;
    while (!toggles.empty() && lv.game_time - toggles.front().when > REDSTONE_TORCH_RECENT_TOGGLE_TIMER) {
        toggles.erase(toggles.begin());
    }
    if (state_get(st, PROP_LIT)) {
        if (neighbor_signal) {
            level_set_block(lv, pos, state_set(st, PROP_LIT, 0), UPDATE_FLAG_ALL);
            if (redstone_torch_is_toggled_too_frequently(lv, pos, true)) {
                // the burnout level event is only sound and particles; the tick is for whatever block is there now
                level_schedule_tick(lv, pos, state_block(level_get_state(lv, pos)), REDSTONE_TORCH_RESTART_DELAY);
            }
        }
    } else if (!neighbor_signal && !redstone_torch_is_toggled_too_frequently(lv, pos, false)) {
        level_set_block(lv, pos, state_set(st, PROP_LIT, 1), UPDATE_FLAG_ALL);
    }
}

void redstone_torch_neighbor_changed(level & lv, state_id st, block_pos pos) {
    int32_t block = state_block(st);
    if (state_get(st, PROP_LIT) == redstone_torch_has_neighbor_signal(lv, st, pos) &&
        !level_ticks_will_tick_this_tick(lv.block_ticks, pos, block)) {
        level_schedule_tick(lv, pos, block, REDSTONE_TORCH_TOGGLE_DELAY);
    }
}

int32_t redstone_torch_get_direct_signal(level & lv, state_id st, block_pos pos, direction dir) {
    return dir == DIRECTION_DOWN ? block_get_signal(lv, st, pos, dir) : 0;
}
