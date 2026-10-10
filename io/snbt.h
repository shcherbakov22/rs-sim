#pragma once

#include "core/common.h"

#include <string>
#include <string_view>
#include <vector>

// Stringified NBT as Tag.toString writes it, for block entity data in case files.
enum snbt_type {
    SNBT_BYTE,
    SNBT_SHORT,
    SNBT_INT,
    SNBT_LONG,
    SNBT_FLOAT,
    SNBT_DOUBLE,
    SNBT_STRING,
    SNBT_LIST,  // also the typed arrays [B;...], [I;...] and [L;...]
    SNBT_COMPOUND,
};

struct snbt_entry;

struct snbt_value {
    snbt_type               type = SNBT_COMPOUND;
    int64_t                 i    = 0;  // integer types
    double                  d    = 0;  // float and double
    std::string             s;
    std::vector<snbt_value> list;
    std::vector<snbt_entry> compound;  // in written order
};

struct snbt_entry {
    std::string key;
    snbt_value  value;
};

// err names the offset on failure
bool snbt_parse(std::string_view text, snbt_value & out, std::string & err);
