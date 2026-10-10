#include "core/trace.h"

#include <cstdarg>

bool trace_wants(const trace * tr, const block_pos * pos) {
    if (tr == nullptr || tr->boundary) return false;
    if (pos == nullptr) return true;
    return pos->x >= tr->box_min.x && pos->x <= tr->box_max.x && pos->y >= tr->box_min.y && pos->y <= tr->box_max.y &&
           pos->z >= tr->box_min.z && pos->z <= tr->box_max.z;
}

static void trace_vadd(trace * tr, const char * fmt, va_list args) {
    if (!tr->tick_shown) {
        tr->lines.push_back("tick " + std::to_string(tr->tick));
        tr->tick_shown = true;
    }
    if (!tr->phase_shown) {
        tr->lines.push_back(std::string(" ") + tr->phase);
        tr->phase_shown = true;
    }
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    std::string line(2 + 2 * (tr->depth > 0 ? tr->depth : 0), ' ');
    line += buf;
    tr->lines.push_back(line);
}

void trace_add(trace * tr, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    trace_vadd(tr, fmt, args);
    va_end(args);
}

void trace_begin_tick(trace * tr) {
    if (tr == nullptr) return;
    tr->tick++;
    tr->tick_shown = false;
    tr->depth      = 0;
    trace_phase(tr, "start");
}

void trace_phase(trace * tr, const char * name) {
    if (tr == nullptr) return;
    tr->phase       = name;
    tr->phase_shown = false;
}

void trace_enter(trace * tr, const block_pos * pos, const char * fmt, ...) {
    if (tr == nullptr) return;
    if (trace_wants(tr, pos)) {
        va_list args;
        va_start(args, fmt);
        trace_vadd(tr, fmt, args);
        va_end(args);
    }
    tr->depth++;
}

void trace_exit(trace * tr) {
    if (tr == nullptr) return;
    tr->depth--;
}

void trace_boundary(trace * tr, const char * fmt, ...) {
    if (!trace_wants(tr, nullptr)) return;
    va_list args;
    va_start(args, fmt);
    trace_vadd(tr, fmt, args);
    va_end(args);
    tr->boundary = true;
}

std::string trace_strip(const std::string & line) {
    size_t hash = line.find("  # ");
    return hash == std::string::npos ? line : line.substr(0, hash);
}
