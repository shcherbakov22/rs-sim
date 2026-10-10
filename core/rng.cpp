#include "core/rng.h"

#include "core/trace.h"

static constexpr int64_t RNG_MULTIPLIER = 25214903917LL;
static constexpr int64_t RNG_MASK       = (1LL << 48) - 1;

void rng_set_seed(rng & r, int64_t seed) {
    r.seed         = (seed ^ RNG_MULTIPLIER) & RNG_MASK;
    r.has_gaussian = false;
}

int32_t rng_next(rng & r, int32_t bits) {
    r.seed        = (int64_t) ((uint64_t) r.seed * RNG_MULTIPLIER + 11) & RNG_MASK;
    int32_t value = (int32_t) (r.seed >> (48 - bits));
    if (trace_wants(r.tr, nullptr)) {
        trace_add(r.tr, "rng %d %d", bits, value);
    }
    return value;
}

int32_t rng_next_int(rng & r) {
    return rng_next(r, 32);
}

int32_t rng_next_int(rng & r, int32_t bound) {
    TMC_ASSERT(bound > 0);
    if ((bound & (bound - 1)) == 0) {
        return (int32_t) ((int64_t) bound * rng_next(r, 31) >> 31);
    }
    int32_t sample;
    int32_t modulo;
    do {
        sample = rng_next(r, 31);
        modulo = sample % bound;
        // Java int overflow: the sum wraps negative for samples in the biased tail
    } while ((int32_t) ((uint32_t) (sample - modulo) + (uint32_t) (bound - 1)) < 0);
    return modulo;
}

int64_t rng_next_long(rng & r) {
    int32_t upper = rng_next(r, 32);
    int32_t lower = rng_next(r, 32);
    return (int64_t) ((uint64_t) (int64_t) upper << 32) + lower;
}

bool rng_next_boolean(rng & r) {
    return rng_next(r, 1) != 0;
}

float rng_next_float(rng & r) {
    return (float) rng_next(r, 24) * 5.9604645E-8F;
}

double rng_next_double(rng & r) {
    int32_t upper    = rng_next(r, 26);
    int32_t lower    = rng_next(r, 27);
    int64_t combined = ((int64_t) upper << 27) + lower;
    // vanilla multiplies by a float constant, so the product is rounded to float
    return (double) ((float) combined * 1.110223E-16F);
}
