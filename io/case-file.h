#pragma once

#include "core/common.h"
#include "core/direction.h"
#include "core/state.h"

#include <string>
#include <vector>

// Port of the oracle's CaseFile/CaseFormat (tmc-case 2). Positions are relative to the case origin.

enum case_verb {
    CASE_VERB_PLACE,
    CASE_VERB_BREAK,
    CASE_VERB_PULL,
};

struct case_input {
    int32_t   at;  // applied after this many ticks, between ticks
    case_verb verb;
    block_pos pos;
    state_id  state = STATE_AIR;  // place
};

// a stack in a block entity: Items by slot, or the RecordItem of a jukebox or the item of a decorated pot at slot 0
struct case_item {
    block_pos pos;
    int32_t   slot;
    int32_t   item;
    int32_t   count;
    int32_t   damage;  // the DAMAGE component, -1 for none
};

// a field of the block entity NBT with its value as Tag.toString writes it; hidden is the oracle's HiddenState
struct case_data {
    block_pos   pos;
    std::string key;
    std::string value;
    bool        hidden;
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
    int64_t                  seed = 0;
    std::vector<state_id>    states;  // index x + size.x * (z + size.z * y)
    std::vector<case_item>   items;
    std::vector<case_data>   data;
    std::vector<case_tick>   ticks;     // in execution order
    std::vector<case_event>  events;    // in execution order
    std::vector<case_toggle> toggles;   // redstone torch burnout history, oldest first
    std::vector<block_pos>   tickers;   // block entity tick order
    std::vector<case_input>  inputs;
    int32_t                  run = 40;  // ticks to trace
};

// err gets "line N: ..." on failure
bool case_parse(const std::string & text, case_file & out, std::string & err);
