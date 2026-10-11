#include "core/shape.h"

#include <algorithm>
#include <cmath>

shape_box block_box(double x1, double y1, double z1, double x2, double y2, double z2) {
    return {
        { x1 / 16, y1 / 16, z1 / 16 },
        { x2 / 16, y2 / 16, z2 / 16 }
    };
}

shape shape_block() {
    return { { { { 0.0, 0.0, 0.0 }, { 1.0, 1.0, 1.0 } } } };
}

shape shape_from_table(uint16_t id) {
    TMC_ASSERT(id != SHAPE_NONE);
    const shape_def & def = shape_defs[id];
    return { std::vector<shape_box>(shape_boxes + def.boxes_first, shape_boxes + def.boxes_first + def.n_boxes) };
}

shape shape_collision(state_id st) {
    return shape_from_table(state_collision_shapes[st]);
}

shape shape_move(shape s, double dx, double dy, double dz) {
    const double d[3] = { dx, dy, dz };
    for (shape_box & b : s.boxes) {
        for (int32_t axis = 0; axis < 3; axis++) {
            b.min[axis] += d[axis];
            b.max[axis] += d[axis];
        }
    }
    return s;
}

// the 1/32 cells from lo to hi as bits, clamped to the block
static uint32_t shape_face_mask(double lo, double hi) {
    TMC_ASSERT(lo * 32 == std::round(lo * 32) && hi * 32 == std::round(hi * 32));
    int32_t a = std::clamp((int32_t) std::lround(lo * 32), 0, 32);
    int32_t b = std::clamp((int32_t) std::lround(hi * 32), 0, 32);
    return a >= b ? 0 : (uint32_t) ((((uint64_t) 1 << (b - a)) - 1) << a);
}

static void shape_face_axes(int32_t axis, int32_t & u, int32_t & v) {
    u = axis == 0 ? 1 : 0;
    v = axis == 2 ? 1 : 2;
}

// the cells just inside the face; a cell of the shape there is a box that spans the sample point
shape_face shape_get_face(const shape & s, direction dir) {
    shape_face f = { direction_get_axis(dir), {}, false };
    int32_t    u;
    int32_t    v;
    shape_face_axes(f.axis, u, v);
    const block_pos & step = DIRECTION_STEPS[dir];
    double            p    = step.x + step.y + step.z > 0 ? 0.9999999 : 1.0E-7;
    for (const shape_box & b : s.boxes) {
        if (b.min[f.axis] > p || b.max[f.axis] <= p) continue;
        f.outside |= b.min[u] < 0.0 || b.max[u] > 1.0 || b.min[v] < 0.0 || b.max[v] > 1.0;
        uint32_t mask = shape_face_mask(b.min[v], b.max[v]);
        for (int32_t i = 0; i < 32; i++) {
            if (shape_face_mask(b.min[u], b.max[u]) >> i & 1) f.rows[i] |= mask;
        }
    }
    return f;
}

bool shape_face_is_full(const shape_face & f) {
    for (uint32_t row : f.rows) {
        if (row != UINT32_MAX) return false;
    }
    return !f.outside;
}

bool shape_face_covers(const shape_face & f, const shape_box & b) {
    int32_t u;
    int32_t v;
    shape_face_axes(f.axis, u, v);
    uint32_t mask = shape_face_mask(b.min[v], b.max[v]);
    for (int32_t i = 0; i < 32; i++) {
        if ((shape_face_mask(b.min[u], b.max[u]) >> i & 1) && (f.rows[i] & mask) != mask) return false;
    }
    return true;
}
