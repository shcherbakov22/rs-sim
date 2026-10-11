#pragma once

#include "core/common.h"
#include "core/state.h"

#include <cmath>

// Vec3, in world coordinates for entities: doubles round like vanilla's only at the same magnitudes
struct vec3 {
    double x;
    double y;
    double z;
};

inline double vec3_get(const vec3 & v, int32_t axis) { return axis == 0 ? v.x : axis == 1 ? v.y : v.z; }

inline vec3 vec3_with(vec3 v, int32_t axis, double value) {
    (axis == 0 ? v.x : axis == 1 ? v.y : v.z) = value;
    return v;
}

inline vec3 vec3_add(const vec3 & a, const vec3 & b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }

inline vec3 vec3_subtract(const vec3 & a, const vec3 & b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }

inline vec3 vec3_scale(const vec3 & v, double s) { return { v.x * s, v.y * s, v.z * s }; }

inline vec3 vec3_multiply(const vec3 & a, const vec3 & b) { return { a.x * b.x, a.y * b.y, a.z * b.z }; }

inline double vec3_length_sqr(const vec3 & v) { return v.x * v.x + v.y * v.y + v.z * v.z; }

inline double vec3_horizontal_distance_sqr(const vec3 & v) { return v.x * v.x + v.z * v.z; }

inline double vec3_horizontal_distance(const vec3 & v) { return std::sqrt(v.x * v.x + v.z * v.z); }

// Vec3.equals: Double.compare per coordinate, so -0.0 differs from 0.0
bool vec3_equals(const vec3 & a, const vec3 & b);
vec3 vec3_normalize(const vec3 & v);

// Direction.axisStepOrder: Y first, then the axis with the larger movement (X on a tie)
void vec3_axis_step_order(const vec3 & movement, int32_t order[3]);

// Mth.floor
inline int32_t floor_int(double v) {
    int32_t i = (int32_t) v;
    return v < i ? i - 1 : i;
}

// Mth.sign
inline int32_t sign_int(double v) { return v == 0.0 ? 0 : v > 0.0 ? 1 : -1; }

// Mth.frac
inline double frac(double v) { return v - (double) (int64_t) std::floor(v); }

// Mth.sin and Mth.cos: a lookup in the table of 65536 float sines
float  mth_sin(double v);
float  mth_cos(double v);
// Mth.atan2: a table lookup refined by a series
double mth_atan2(double y, double x);
// Mth.wrapDegrees(float)
float  wrap_degrees(float angle);

// AABB: the constructor sorts each axis
aabb aabb_of(double x0, double y0, double z0, double x1, double y1, double z1);
aabb aabb_move(const aabb & b, const vec3 & d);
aabb aabb_inflate(const aabb & b, double x, double y, double z);
aabb aabb_expand_towards(const aabb & b, const vec3 & d);
aabb aabb_minmax(const aabb & a, const aabb & b);
bool aabb_intersects(const aabb & a, const aabb & b);
bool aabb_contains(const aabb & b, const vec3 & v);
vec3 aabb_get_center(const aabb & b);
// AABB.clip: whether the segment enters the box
bool aabb_clip(const aabb & b, const vec3 & from, const vec3 & to, vec3 * hit);
// VoxelShape.collide of Shapes.create(b): one cell at the box's own coordinates
double aabb_collide(const aabb & b, int32_t axis, const aabb & moving, double distance);
// AABB.collidedAlongVector against one box
bool aabb_collided_along_vector(const aabb & b, const vec3 & vector, const aabb & part);
