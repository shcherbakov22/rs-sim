#include "engine/level.h"

#include "core/trace.h"

#include <cstdarg>

static int64_t section_key(block_pos pos) {
    // SectionPos.asLong layout
    int64_t x = pos.x >> 4;
    int64_t y = pos.y >> 4;
    int64_t z = pos.z >> 4;
    return (x & 0x3FFFFF) << 42 | (z & 0x3FFFFF) << 20 | (y & 0xFFFFF);
}

static int32_t section_index(block_pos pos) {
    return (pos.y & 15) << 8 | (pos.z & 15) << 4 | (pos.x & 15);
}

state_id level_get_state(level & lv, block_pos pos) {
    if (pos.y < LEVEL_Y_MIN || pos.y > LEVEL_Y_MAX) {
        // vanilla answers void_air here
        level_unsupported(lv, "read outside build height");
        return STATE_AIR;
    }
    auto it = lv.sections.find(section_key(pos));
    return it == lv.sections.end() ? STATE_AIR : it->second->states[section_index(pos)];
}

void level_set_state_raw(level & lv, block_pos pos, state_id st) {
    TMC_ASSERT(pos.y >= LEVEL_Y_MIN && pos.y <= LEVEL_Y_MAX);
    std::unique_ptr<level_section> & s = lv.sections[section_key(pos)];
    if (!s) {
        s = std::make_unique<level_section>();
    }
    s->states[section_index(pos)] = st;
}

void level_unsupported(level & lv, const char * fmt, ...) {
    if (!lv.unsupported.empty()) {
        return;
    }
    char    buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    lv.unsupported = buf;
}

void level_tick(level & lv) {
    trace_begin_tick(lv.tr);
    // world border, weather, sleep, sky: off by game rules or no-ops for a void world
    lv.game_time++;

    trace_phase(lv.tr, "blockTicks");
    trace_phase(lv.tr, "fluidTicks");
    trace_phase(lv.tr, "raid");
    trace_phase(lv.tr, "chunkSource");
    trace_phase(lv.tr, "blockEvents");
    trace_phase(lv.tr, "entities");
    trace_phase(lv.tr, "blockEntities");
    trace_phase(lv.tr, "entityManagement");
    trace_phase(lv.tr, "between");
}
