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

// Mth.SIN
static float mth_sin_table(int64_t i) { return (float) std::sin((double) (i & 65535) / 10430.378350470453); }

float mth_sin(double v) { return mth_sin_table((int64_t) (v * 10430.378350470453)); }

float mth_cos(double v) { return mth_sin_table((int64_t) (v * 10430.378350470453 + 16384.0)); }

double mth_atan2(double y, double x) {
    double d2 = x * x + y * y;
    if (std::isnan(d2)) return NAN;
    bool neg_y = y < 0.0;
    if (neg_y) y = -y;
    bool neg_x = x < 0.0;
    if (neg_x) x = -x;
    bool steep = y > x;
    if (steep) std::swap(x, y);
    // Mth.fastInvSqrt
    double rinv = std::bit_cast<double>(6910469410427058090LL - (std::bit_cast<int64_t>(d2) >> 1));
    rinv        = rinv * (1.5 - 0.5 * d2 * rinv * rinv);
    x *= rinv;
    y *= rinv;
    // FRAC_BIAS puts y in 1/256 steps into the low bits; ASIN_TAB and COS_TAB hold asin and its cosine per step
    double  frac_bias = std::bit_cast<double>(4805340802404319232LL);
    double  yp        = frac_bias + y;
    int32_t index     = (int32_t) std::bit_cast<int64_t>(yp);
    double  phi       = std::asin(index / 256.0);
    double  c_phi     = std::cos(phi);
    double  s_phi     = yp - frac_bias;
    double  sd        = y * c_phi - x * s_phi;
    double  d         = (6.0 + sd * sd) * sd * 0.16666666666666666;
    double  theta     = phi + d;
    if (steep) theta = 3.141592653589793 / 2 - theta;
    if (neg_x) theta = 3.141592653589793 - theta;
    if (neg_y) theta = -theta;
    return theta;
}

float wrap_degrees(float angle) {
    float normalized = std::fmod(angle, 360.0F);
    if (normalized >= 180.0F) normalized -= 360.0F;
    if (normalized < -180.0F) normalized += 360.0F;
    return normalized;
}

void vec3_axis_step_order(const vec3 & movement, int32_t order[3]) {
    bool z_first = std::abs(movement.x) < std::abs(movement.z);
    order[0]     = 1;
    order[1]     = z_first ? 2 : 0;
    order[2]     = z_first ? 0 : 2;
}

aabb aabb_of_size(const vec3 & center, double x, double y, double z) {
    return aabb_of(center.x - x / 2.0, center.y - y / 2.0, center.z - z / 2.0, center.x + x / 2.0, center.y + y / 2.0,
                   center.z + z / 2.0);
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

// VoxelShape.findIndex on one cell: moving overlaps it on the other two axes and lies before it along a
double aabb_collide(const aabb & box, int32_t a, const aabb & moving, double distance) {
    if (std::fabs(distance) < 1.0E-7) return 0.0;
    for (int32_t other : { (a + 1) % 3, (a + 2) % 3 }) {
        if (!(moving.min[other] + 1.0E-7 < box.max[other]) || !(moving.max[other] - 1.0E-7 >= box.min[other])) {
            return distance;
        }
    }
    if (distance > 0.0) {
        double gap = box.min[a] - moving.max[a];
        if (moving.max[a] - 1.0E-7 < box.min[a] && gap >= -1.0E-7) distance = std::min(distance, gap);
    } else {
        double gap = box.max[a] - moving.min[a];
        if (moving.min[a] + 1.0E-7 >= box.max[a] && gap <= 1.0E-7) distance = std::max(distance, gap);
    }
    return distance;
}

// Mth.lerp(0.5, min, max) per axis
vec3 aabb_get_center(const aabb & b) {
    return { b.min[0] + 0.5 * (b.max[0] - b.min[0]), b.min[1] + 0.5 * (b.max[1] - b.min[1]),
             b.min[2] + 0.5 * (b.max[2] - b.min[2]) };
}

// AABB.clipPoint
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

bool aabb_clip_face(const aabb & b, const vec3 & from, const vec3 & to, double & scale, direction & face) {
    static constexpr direction MIN_FACE[3] = { DIRECTION_WEST, DIRECTION_DOWN, DIRECTION_NORTH };
    static constexpr direction MAX_FACE[3] = { DIRECTION_EAST, DIRECTION_UP, DIRECTION_SOUTH };
    double d[3]    = { to.x - from.x, to.y - from.y, to.z - from.z };
    double f[3]    = { from.x, from.y, from.z };
    bool   clipped = false;
    for (int32_t a = 0; a < 3; a++) {
        if (d[a] > 1.0E-7 && aabb_clip_point(scale, d, f, a, b.min[a], b)) {
            face    = MIN_FACE[a];
            clipped = true;
        } else if (d[a] < -1.0E-7 && aabb_clip_point(scale, d, f, a, b.max[a], b)) {
            face    = MAX_FACE[a];
            clipped = true;
        }
    }
    return clipped;
}

bool aabb_clip(const aabb & b, const vec3 & from, const vec3 & to, vec3 * hit) {
    double    scale = 1.0;
    direction face  = DIRECTION_DOWN;
    if (!aabb_clip_face(b, from, to, scale, face)) return false;
    if (hit != nullptr) *hit = vec3_add(from, vec3_scale(vec3_subtract(to, from), scale));
    return true;
}

bool aabb_collided_along_vector(const aabb & b, const vec3 & vector, const aabb & part) {
    vec3 from     = aabb_get_center(b);
    vec3 to       = vec3_add(from, vector);
    aabb inflated = aabb_inflate(part, (b.max[0] - b.min[0]) * 0.5 - 1.0E-7, (b.max[1] - b.min[1]) * 0.5 - 1.0E-7,
                                 (b.max[2] - b.min[2]) * 0.5 - 1.0E-7);
    return aabb_contains(inflated, to) || aabb_contains(inflated, from) || aabb_clip(inflated, from, to, nullptr);
}
