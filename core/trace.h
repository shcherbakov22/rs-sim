#pragma once

#include "core/common.h"

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
    bool                     boundary    = false;  // past an entity spawn, explosion or fluid tick nothing is contract
};

// false if tracing is off, past a boundary, or pos (when given) is outside the box
bool trace_wants(const trace * tr, const block_pos * pos);

// appends a line at the current depth; call only after trace_wants
void trace_add(trace * tr, const char * fmt, ...) __attribute__((format(printf, 2, 3)));

void trace_begin_tick(trace * tr);
void trace_phase(trace * tr, const char * name);

// enter logs like trace_add when wanted; depth changes either way
void trace_enter(trace * tr, const block_pos * pos, const char * fmt, ...) __attribute__((format(printf, 3, 4)));
void trace_exit(trace * tr);

void trace_boundary(trace * tr, const char * fmt, ...) __attribute__((format(printf, 2, 3)));

// annotations ("  # ...") are not part of the contract
std::string trace_strip(const std::string & line);
