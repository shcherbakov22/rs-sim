#include "engine/case-run.h"

#include "engine/level.h"

// Batch.SURROUND: the oracle clears and traces this margin around the region
static constexpr int32_t CASE_SURROUND = 13;

// Snapshot.restore: silent placement, then pending ticks, block events and block entity tickers
static void case_place(level & lv, const case_file & f) {
    for (int32_t y = 0; y < f.size.y; y++) {
        for (int32_t z = 0; z < f.size.z; z++) {
            for (int32_t x = 0; x < f.size.x; x++) {
                level_set_state_raw(lv, { x, y, z }, f.states[x + f.size.x * (z + f.size.z * y)]);
            }
        }
    }
    if (!f.nbt.empty() || !f.tickers.empty()) {
        level_unsupported(lv, "block entities");
    }
    if (!f.ticks.empty()) {
        level_unsupported(lv, "scheduled ticks in case");
    }
    if (!f.events.empty()) {
        level_unsupported(lv, "block events in case");
    }
    rng_set_seed(lv.random, f.seed);
    lv.random.has_gaussian = f.has_gaussian;
    lv.random.gaussian     = f.gaussian;
}

static void case_apply_inputs(level & lv, const case_file & f, int32_t gap) {
    for (const case_input & in : f.inputs) {
        if (in.at == gap) {
            level_unsupported(lv, "inputs");
        }
    }
}

bool case_run(const case_file & f, trace & tr, std::string & err) {
    level lv;
    case_place(lv, f);

    tr           = trace();
    tr.box_min   = { -CASE_SURROUND, -CASE_SURROUND, -CASE_SURROUND };
    tr.box_max   = { f.size.x + CASE_SURROUND - 1, f.size.y + CASE_SURROUND - 1, f.size.z + CASE_SURROUND - 1 };
    lv.tr        = &tr;
    lv.random.tr = &tr;

    case_apply_inputs(lv, f, 0);
    // a boundary ends the run at the end of the gap where it happened
    int32_t gap = 0;
    while (lv.unsupported.empty() && !tr.boundary) {
        level_tick(lv);
        if (tr.boundary || ++gap >= f.run) {
            break;
        }
        case_apply_inputs(lv, f, gap);
    }
    if (!lv.unsupported.empty()) {
        err = "unsupported: " + lv.unsupported;
        return false;
    }
    return true;
}
