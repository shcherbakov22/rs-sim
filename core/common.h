#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define TMC_ABORT(...)                                  \
    do {                                                \
        fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                   \
        fprintf(stderr, "\n");                          \
        abort();                                        \
    } while (0)

#define TMC_ASSERT(x)                               \
    do {                                            \
        if (!(x)) {                                 \
            TMC_ABORT("TMC_ASSERT(%s) failed", #x); \
        }                                           \
    } while (0)

struct block_pos {
    int32_t x;
    int32_t y;
    int32_t z;
};

inline bool operator==(const block_pos & a, const block_pos & b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
