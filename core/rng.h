#pragma once

#include "core/common.h"

struct trace;

// Port of LegacyRandomSource (BitRandomSource defaults); nothing in scope draws a Gaussian sample.
struct rng {
    int64_t seed = 0;        // 48-bit LCG state
    trace * tr   = nullptr;  // set for the level RNG: every draw is traced
};

void    rng_set_seed(rng & r, int64_t seed);
int32_t rng_next(rng & r, int32_t bits);
int32_t rng_next_int(rng & r);
int32_t rng_next_int(rng & r, int32_t bound);
int64_t rng_next_long(rng & r);
bool    rng_next_boolean(rng & r);
float   rng_next_float(rng & r);
double  rng_next_double(rng & r);
