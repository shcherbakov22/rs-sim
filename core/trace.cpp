#include "core/trace.h"

#include <charconv>
#include <cmath>
#include <cstdarg>

bool trace_wants(const trace * tr, const block_pos * pos) {
    if (tr == nullptr) return false;
    if (pos == nullptr) return true;
    return pos->x >= tr->box_min.x && pos->x <= tr->box_max.x && pos->y >= tr->box_min.y && pos->y <= tr->box_max.y &&
           pos->z >= tr->box_min.z && pos->z <= tr->box_max.z;
}

void trace_vadd(trace * tr, const char * fmt, va_list args) {
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

void trace_enter(trace * tr) {
    if (tr != nullptr) tr->depth++;
}

void trace_exit(trace * tr) {
    if (tr != nullptr) tr->depth--;
}

std::string trace_strip(const std::string & line) {
    size_t hash = line.find("  # ");
    return hash == std::string::npos ? line : line.substr(0, hash);
}

std::string trace_double(double v) {
    if (v == 0.0) return std::signbit(v) ? "-0.0" : "0.0";
    char        buf[64];
    // shortest round trip, as d.ddde+xx
    char *      end = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::scientific).ptr;
    std::string text(buf, end);
    std::string sign = v < 0.0 ? "-" : "";
    size_t      e    = text.find('e');
    int32_t     exp  = atoi(text.c_str() + e + 1);
    std::string digits;
    for (size_t i = sign.size(); i < e; i++) {
        if (text[i] != '.') digits += text[i];
    }
    int32_t n = (int32_t) digits.size();
    if (std::fabs(v) >= 1.0E-3 && std::fabs(v) < 1.0E7) {
        int32_t point = exp + 1;  // digits before the decimal point
        if (point <= 0) return sign + "0." + std::string(-point, '0') + digits;
        if (point >= n) return sign + digits + std::string(point - n, '0') + ".0";
        return sign + digits.substr(0, point) + "." + digits.substr(point);
    }
    return sign + digits.substr(0, 1) + "." + (n > 1 ? digits.substr(1) : "0") + "E" + std::to_string(exp);
}
