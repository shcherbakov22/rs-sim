#include "core/aabb.h"

#include <algorithm>
#include <bit>
#include <cmath>

bool vec3_equals(const vec3 & a, const vec3 & b) {
    // Double.compare is equality of the bits once NaN is normalized, and no position is NaN
    return std::bit_cast<uint64_t>(a.x) == std::bit_cast<uint64_t>(b.x) &&
           std::bit_cast<uint64_t>(a.y) == std::bit_cast<uint64_t>(b.y) &&
           std::bit_cast<uint64_t>(a.z) == std::bit_cast<uint64_t>(b.z);
}

vec3 vec3_normalize(const vec3 & v) {
    double dist = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return dist < 1.0E-5F ? vec3{} : vec3{ v.x / dist, v.y / dist, v.z / dist };
}

void vec3_axis_step_order(const vec3 & movement, int32_t order[3]) {
    bool z_first = std::abs(movement.x) < std::abs(movement.z);
    order[0]     = 1;
    order[1]     = z_first ? 2 : 0;
    order[2]     = z_first ? 0 : 2;
}

aabb aabb_of(double x0, double y0, double z0, double x1, double y1, double z1) {
    return {
        { std::min(x0, x1), std::min(y0, y1), std::min(z0, z1) },
        { std::max(x0, x1), std::max(y0, y1), std::max(z0, z1) }
    };
}

aabb aabb_move(const aabb & b, const vec3 & d) {
    return aabb_of(b.min[0] + d.x, b.min[1] + d.y, b.min[2] + d.z, b.max[0] + d.x, b.max[1] + d.y, b.max[2] + d.z);
}

aabb aabb_inflate(const aabb & b, double x, double y, double z) {
    return aabb_of(b.min[0] - x, b.min[1] - y, b.min[2] - z, b.max[0] + x, b.max[1] + y, b.max[2] + z);
}

aabb aabb_expand_towards(const aabb & b, const vec3 & d) {
    aabb r = b;
    for (int32_t axis = 0; axis < 3; axis++) {
        double v = vec3_get(d, axis);
        if (v < 0.0) {
            r.min[axis] += v;
        } else if (v > 0.0) {
            r.max[axis] += v;
        }
    }
    return aabb_of(r.min[0], r.min[1], r.min[2], r.max[0], r.max[1], r.max[2]);
}

aabb aabb_minmax(const aabb & a, const aabb & b) {
    return aabb_of(std::min(a.min[0], b.min[0]), std::min(a.min[1], b.min[1]), std::min(a.min[2], b.min[2]),
                   std::max(a.max[0], b.max[0]), std::max(a.max[1], b.max[1]), std::max(a.max[2], b.max[2]));
}

bool aabb_intersects(const aabb & a, const aabb & b) {
    return a.min[0] < b.max[0] && a.max[0] > b.min[0] && a.min[1] < b.max[1] && a.max[1] > b.min[1] &&
           a.min[2] < b.max[2] && a.max[2] > b.min[2];
}

bool aabb_contains(const aabb & b, const vec3 & v) {
    return v.x >= b.min[0] && v.x < b.max[0] && v.y >= b.min[1] && v.y < b.max[1] && v.z >= b.min[2] && v.z < b.max[2];
}

// Mth.lerp(0.5, min, max) per axis
vec3 aabb_get_center(const aabb & b) {
    return { b.min[0] + 0.5 * (b.max[0] - b.min[0]), b.min[1] + 0.5 * (b.max[1] - b.min[1]),
             b.min[2] + 0.5 * (b.max[2] - b.min[2]) };
}

// AABB.clipPoint: the face plane at point along axis a, hit within the face (b and c) closer than scale
static bool aabb_clip_point(double & scale, const double d[3], const double from[3], int32_t a, double point,
                            const aabb & box) {
    int32_t b  = (a + 1) % 3;
    int32_t c  = (a + 2) % 3;
    double  s  = (point - from[a]) / d[a];
    double  pb = from[b] + s * d[b];
    double  pc = from[c] + s * d[c];
    if (0.0 < s && s < scale && box.min[b] - 1.0E-7 < pb && pb < box.max[b] + 1.0E-7 && box.min[c] - 1.0E-7 < pc &&
        pc < box.max[c] + 1.0E-7) {
        scale = s;
        return true;
    }
    return false;
}

bool aabb_clip(const aabb & b, const vec3 & from, const vec3 & to, vec3 * hit) {
    double scale   = 1.0;
    double d[3]    = { to.x - from.x, to.y - from.y, to.z - from.z };
    double f[3]    = { from.x, from.y, from.z };
    bool   clipped = false;
    for (int32_t a = 0; a < 3; a++) {
        if (d[a] > 1.0E-7) {
            clipped |= aabb_clip_point(scale, d, f, a, b.min[a], b);
        } else if (d[a] < -1.0E-7) {
            clipped |= aabb_clip_point(scale, d, f, a, b.max[a], b);
        }
    }
    if (clipped && hit != nullptr) *hit = { from.x + scale * d[0], from.y + scale * d[1], from.z + scale * d[2] };
    return clipped;
}

bool aabb_collided_along_vector(const aabb & b, const vec3 & vector, const aabb & part) {
    vec3 from     = aabb_get_center(b);
    vec3 to       = vec3_add(from, vector);
    aabb inflated = aabb_inflate(part, (b.max[0] - b.min[0]) * 0.5 - 1.0E-7, (b.max[1] - b.min[1]) * 0.5 - 1.0E-7,
                                 (b.max[2] - b.min[2]) * 0.5 - 1.0E-7);
    return aabb_contains(inflated, to) || aabb_contains(inflated, from) || aabb_clip(inflated, from, to, nullptr);
}
