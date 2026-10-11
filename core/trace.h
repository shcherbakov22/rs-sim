#pragma once

#include "core/common.h"

#include <cstdarg>
#include <string>
#include <vector>

// Port of the oracle's Trace: lines grouped by tick and phase, indented by call depth.
// Positions are relative to the case origin. Events outside the box are dropped.
struct trace {
    std::vector<std::string> lines;
    block_pos                box_min;
    block_pos                box_max;  // inclusive
    int32_t                  tick        = 0;
    int32_t                  depth       = 0;
    const char *             phase       = "between";
    bool                     tick_shown  = false;
    bool                     phase_shown = false;
    bool                     record      = true;  // false: tracing off, and the level runs without a trace
};

// false without a trace (tracing off or past a boundary), or if pos (when given) is outside the box
bool trace_wants(const trace * tr, const block_pos * pos);

// appends a line at the current depth; call only after trace_wants
void trace_add(trace * tr, const char * fmt, ...) __attribute__((format(printf, 2, 3)));
void trace_vadd(trace * tr, const char * fmt, va_list args);

void trace_begin_tick(trace * tr);
void trace_phase(trace * tr, const char * name);

void trace_enter(trace * tr);
void trace_exit(trace * tr);

// Double.toString: the shortest digits that read back exactly, plain from 1e-3 to 1e7, else as d.dddE-n
std::string trace_double(double v);

// annotations ("  # ...") are not part of the contract
std::string trace_strip(const std::string & line);
