#pragma once

#include "core/common.h"
#include "core/direction.h"
#include "core/state.h"

#include <string>
#include <vector>

// Port of the oracle's CaseFile/CaseFormat (tmc-case 1). Positions are relative to the case origin.

enum case_verb {
    CASE_VERB_USE,
    CASE_VERB_ATTACK,
    CASE_VERB_PLACE,
    CASE_VERB_BREAK,
    CASE_VERB_CLOSE,
};

struct case_input {
    int32_t     at;  // applied after this many ticks, between ticks
    case_verb   verb;
    block_pos   pos;
    state_id    state = STATE_AIR;     // place
    direction   face  = DIRECTION_UP;  // use
    std::string item;                  // use: empty = empty hand
    int32_t     count = 0;
};

// block entity NBT exactly as written in the key; parsed when block entities are simulated
struct case_nbt {
    block_pos   pos;
    std::string snbt;
};

struct case_tick {
    block_pos pos;
    int32_t   block;
    int64_t   delay;
    int32_t   priority;  // tick_priority
};

struct case_event {
    block_pos pos;
    int32_t   block;
    int32_t   param_a;
    int32_t   param_b;
};

// RedstoneTorchBlock.Toggle; when is relative to the case start
struct case_toggle {
    block_pos pos;
    int64_t   when;
};

struct case_file {
    block_pos                size;
    int64_t                  seed         = 0;
    bool                     has_gaussian = false;
    double                   gaussian     = 0;
    std::vector<state_id>    states;    // index x + size.x * (z + size.z * y)
    std::vector<case_nbt>    nbt;
    std::vector<case_tick>   ticks;     // in execution order
    std::vector<case_event>  events;    // in execution order
    std::vector<case_toggle> toggles;   // redstone torch burnout history, oldest first
    std::vector<block_pos>   tickers;   // block entity tick order
    std::vector<case_input>  inputs;
    int32_t                  run = 40;  // ticks to trace
};

// err gets "line N: ..." on failure
bool case_parse(const std::string & text, case_file & out, std::string & err);
