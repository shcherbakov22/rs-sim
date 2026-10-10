#include "core/voxel-shape.h"

#include <algorithm>
#include <cmath>
#include <iterator>

static int32_t voxel_shape_size(const voxel_shape & s, int32_t axis) {
    return s.coords[axis].empty() ? 0 : (int32_t) s.coords[axis].size() - 1;
}

static size_t voxel_shape_cell(const voxel_shape & s, const int32_t c[3]) {
    return ((size_t) c[0] * voxel_shape_size(s, 1) + c[1]) * voxel_shape_size(s, 2) + c[2];
}

// VoxelShape.findIndex: the cell holding coord, -1 below the shape, the size above it
static int32_t voxel_shape_find_index(const voxel_shape & s, int32_t axis, double coord) {
    const std::vector<double> & c = s.coords[axis];
    return (int32_t) (std::upper_bound(c.begin(), c.end(), coord) - c.begin()) - 1;
}

// DiscreteVoxelShape.isFullWide at a point inside a cell
static bool voxel_shape_full_at(const voxel_shape & s, const double p[3]) {
    int32_t c[3];
    for (int32_t axis = 0; axis < 3; axis++) {
        c[axis] = voxel_shape_find_index(s, axis, p[axis]);
        if (c[axis] < 0 || c[axis] >= voxel_shape_size(s, axis)) {
            return false;
        }
    }
    return s.full[voxel_shape_cell(s, c)] != 0;
}

static bool boolean_op_apply(boolean_op op, bool a, bool b) {
    switch (op) {
        case BOOLEAN_OP_OR:
            return a || b;
        case BOOLEAN_OP_ONLY_FIRST:
            return a && !b;
        case BOOLEAN_OP_ONLY_SECOND:
            return b && !a;
        case BOOLEAN_OP_NOT_SAME:
            return a != b;
    }
    TMC_ASSERT(false);
    return false;
}

// a new shape on the given bounds, each cell sampled from a and b at its center and combined by op
static voxel_shape voxel_shape_sample(const std::vector<double> coords[3],
                                      const voxel_shape &       a,
                                      const voxel_shape &       b,
                                      boolean_op                op) {
    voxel_shape out;
    for (int32_t axis = 0; axis < 3; axis++) {
        out.coords[axis] = coords[axis];
    }
    int32_t n[3] = { voxel_shape_size(out, 0), voxel_shape_size(out, 1), voxel_shape_size(out, 2) };
    out.full.assign((size_t) n[0] * n[1] * n[2], 0);
    int32_t c[3];
    for (c[0] = 0; c[0] < n[0]; c[0]++) {
        for (c[1] = 0; c[1] < n[1]; c[1]++) {
            for (c[2] = 0; c[2] < n[2]; c[2]++) {
                double p[3];
                for (int32_t axis = 0; axis < 3; axis++) {
                    p[axis] = (out.coords[axis][c[axis]] + out.coords[axis][c[axis] + 1]) / 2;
                }
                out.full[voxel_shape_cell(out, c)] =
                    boolean_op_apply(op, voxel_shape_full_at(a, p), voxel_shape_full_at(b, p));
            }
        }
    }
    return out;
}

// compares the cells at index i and j along axis; j = -1 compares with nothing
static bool voxel_shape_slabs_equal(const voxel_shape & s, int32_t axis, int32_t i, int32_t j) {
    int32_t n[3] = { voxel_shape_size(s, 0), voxel_shape_size(s, 1), voxel_shape_size(s, 2) };
    n[axis]      = 1;
    int32_t c[3];
    for (c[0] = 0; c[0] < n[0]; c[0]++) {
        for (c[1] = 0; c[1] < n[1]; c[1]++) {
            for (c[2] = 0; c[2] < n[2]; c[2]++) {
                int32_t d[3] = { c[0], c[1], c[2] };
                d[axis]      = i;
                bool a       = s.full[voxel_shape_cell(s, d)] != 0;
                d[axis]      = j;
                bool b       = j >= 0 && s.full[voxel_shape_cell(s, d)] != 0;
                if (a != b) {
                    return false;
                }
            }
        }
    }
    return true;
}

// VoxelShape.optimize, up to representation: trims empty outer cells and drops bounds between equal cells
static voxel_shape voxel_shape_optimize(const voxel_shape & s) {
    if (voxel_shape_is_empty(s)) {
        return voxel_shape();
    }
    voxel_shape cur = s;
    for (int32_t axis = 0; axis < 3; axis++) {
        int32_t first = 0;
        int32_t last  = voxel_shape_size(cur, axis) - 1;
        while (voxel_shape_slabs_equal(cur, axis, first, -1)) {
            first++;
        }
        while (voxel_shape_slabs_equal(cur, axis, last, -1)) {
            last--;
        }
        std::vector<double>   coords[3] = { cur.coords[0], cur.coords[1], cur.coords[2] };
        std::vector<double> & keep      = coords[axis];
        keep.clear();
        keep.push_back(cur.coords[axis][first]);
        for (int32_t i = first + 1; i <= last; i++) {
            if (!voxel_shape_slabs_equal(cur, axis, i - 1, i)) {
                keep.push_back(cur.coords[axis][i]);
            }
        }
        keep.push_back(cur.coords[axis][last + 1]);
        cur = voxel_shape_sample(coords, cur, voxel_shape(), BOOLEAN_OP_OR);
    }
    return cur;
}

voxel_shape shapes_block() {
    return shapes_box(0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
}

voxel_shape shapes_box(double x1, double y1, double z1, double x2, double y2, double z2) {
    voxel_shape s;
    s.coords[0] = { x1, x2 };
    s.coords[1] = { y1, y2 };
    s.coords[2] = { z1, z2 };
    s.full      = { 1 };
    return s;
}

voxel_shape shapes_join(const voxel_shape & a, const voxel_shape & b, boolean_op op) {
    std::vector<double> coords[3];
    for (int32_t axis = 0; axis < 3; axis++) {
        std::merge(a.coords[axis].begin(), a.coords[axis].end(), b.coords[axis].begin(), b.coords[axis].end(),
                   std::back_inserter(coords[axis]));
        coords[axis].erase(std::unique(coords[axis].begin(), coords[axis].end()), coords[axis].end());
    }
    return voxel_shape_optimize(voxel_shape_sample(coords, a, b, op));
}

bool shapes_join_is_not_empty(const voxel_shape & a, const voxel_shape & b, boolean_op op) {
    bool a_empty = voxel_shape_is_empty(a);
    bool b_empty = voxel_shape_is_empty(b);
    if (a_empty || b_empty) {
        return boolean_op_apply(op, !a_empty, !b_empty);
    }
    std::vector<double> coords[3];
    for (int32_t axis = 0; axis < 3; axis++) {
        std::merge(a.coords[axis].begin(), a.coords[axis].end(), b.coords[axis].begin(), b.coords[axis].end(),
                   std::back_inserter(coords[axis]));
        coords[axis].erase(std::unique(coords[axis].begin(), coords[axis].end()), coords[axis].end());
    }
    voxel_shape joined = voxel_shape_sample(coords, a, b, op);
    return !voxel_shape_is_empty(joined);
}

bool voxel_shape_is_empty(const voxel_shape & s) {
    return std::find(s.full.begin(), s.full.end(), 1) == s.full.end();
}

voxel_shape voxel_shape_move(const voxel_shape & s, double dx, double dy, double dz) {
    voxel_shape out  = s;
    double      d[3] = { dx, dy, dz };
    for (int32_t axis = 0; axis < 3; axis++) {
        for (double & c : out.coords[axis]) {
            c += d[axis];
        }
    }
    return out;
}

// VoxelShape.isCubeLikeAlong
static bool voxel_shape_is_cube_like_along(const voxel_shape & s, int32_t axis) {
    const std::vector<double> & c = s.coords[axis];
    return c.size() == 2 && std::abs(c[0] - 0.0) <= 1.0E-7 && std::abs(c[1] - 1.0) <= 1.0E-7;
}

// VoxelShape.getFaceShape: the cells just inside the face, stretched over the whole block along its axis
voxel_shape voxel_shape_get_face_shape(const voxel_shape & s, direction dir) {
    int32_t axis = direction_get_axis(dir);
    if (voxel_shape_is_empty(s) || voxel_shape_is_cube_like_along(s, axis)) {
        return s;
    }
    const block_pos & step  = direction_defs[dir].step;
    int32_t           index = voxel_shape_find_index(s, axis, step.x + step.y + step.z > 0 ? 0.9999999 : 1.0E-7);
    // SliceShape.isEmpty looks only at the extent of the whole shape
    if (index < 0 || index >= voxel_shape_size(s, axis)) {
        return voxel_shape();
    }
    voxel_shape slice;
    for (int32_t a = 0; a < 3; a++) {
        slice.coords[a] = a == axis ? std::vector<double>{ 0.0, 1.0 } : s.coords[a];
    }
    int32_t n[3] = { voxel_shape_size(slice, 0), voxel_shape_size(slice, 1), voxel_shape_size(slice, 2) };
    slice.full.assign((size_t) n[0] * n[1] * n[2], 0);
    int32_t c[3];
    for (c[0] = 0; c[0] < n[0]; c[0]++) {
        for (c[1] = 0; c[1] < n[1]; c[1]++) {
            for (c[2] = 0; c[2] < n[2]; c[2]++) {
                int32_t d[3]                           = { c[0], c[1], c[2] };
                d[axis]                                = index;
                slice.full[voxel_shape_cell(slice, c)] = s.full[voxel_shape_cell(s, d)];
            }
        }
    }
    // an empty slice inside the extent would still count as a shape in vanilla; no block shape has such a gap
    TMC_ASSERT(!voxel_shape_is_empty(slice));
    // SliceShape.isCubeLike
    for (int32_t a = 0; a < 3; a++) {
        if (a != axis && !voxel_shape_is_cube_like_along(slice, a)) {
            return slice;
        }
    }
    return shapes_block();
}

voxel_shape voxel_shape_from_table(uint16_t id) {
    TMC_ASSERT(id != SHAPE_NONE);
    voxel_shape       out;
    const shape_def & def = shape_defs[id];
    for (int32_t i = def.boxes_first; i < def.boxes_first + def.n_boxes; i++) {
        const shape_box & b = shape_boxes[i];
        out = shapes_join(out, shapes_box(b.min[0], b.min[1], b.min[2], b.max[0], b.max[1], b.max[2]), BOOLEAN_OP_OR);
    }
    return out;
}

voxel_shape voxel_shape_collision(state_id st) {
    return voxel_shape_from_table(state_collision_shapes[st]);
}
