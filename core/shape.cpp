#include "core/shape.h"

#include <algorithm>
#include <cmath>

shape shape_block() { return { { { { 0.0, 0.0, 0.0 }, { 1.0, 1.0, 1.0 } } } }; }

shape shape_from_table(uint16_t id) {
    TMC_ASSERT(id != SHAPE_NONE);
    const shape_def & def = shape_defs[id];
    return { std::vector<aabb>(shape_boxes + def.boxes_first, shape_boxes + def.boxes_first + def.n_boxes) };
}

shape shape_collision(state_id st) { return shape_from_table(state_collision_shapes[st]); }

shape shape_move(shape s, double dx, double dy, double dz) {
    const double d[3] = { dx, dy, dz };
    for (aabb & b : s.boxes) {
        for (int32_t axis = 0; axis < 3; axis++) {
            b.min[axis] += d[axis];
            b.max[axis] += d[axis];
        }
    }
    return s;
}

// the 1/32 cell edge at x, clamped to the block; shape edges lie on the 1/32 grid, so the cast is exact
static int32_t shape_face_cell(double x) {
    TMC_ASSERT(x * 32 == std::round(x * 32));
    return std::clamp((int32_t) (x * 32), 0, 32);
}

static uint32_t shape_face_mask(double lo, double hi) {
    int32_t a = shape_face_cell(lo);
    int32_t b = shape_face_cell(hi);
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
    for (const aabb & b : s.boxes) {
        if (b.min[f.axis] > p || b.max[f.axis] <= p) continue;
        f.outside |= b.min[u] < 0.0 || b.max[u] > 1.0 || b.min[v] < 0.0 || b.max[v] > 1.0;
        uint32_t mask = shape_face_mask(b.min[v], b.max[v]);
        int32_t  end  = shape_face_cell(b.max[u]);
        for (int32_t i = shape_face_cell(b.min[u]); i < end; i++) f.rows[i] |= mask;
    }
    return f;
}

bool shape_face_is_full(const shape_face & f) {
    for (uint32_t row : f.rows) {
        if (row != UINT32_MAX) return false;
    }
    return !f.outside;
}

bool shape_face_covers(const shape_face & f, uint16_t shape_id) {
    int32_t u;
    int32_t v;
    shape_face_axes(f.axis, u, v);
    for (const aabb & b : shape_from_table(shape_id).boxes) {
        uint32_t mask = shape_face_mask(b.min[v], b.max[v]);
        int32_t  end  = shape_face_cell(b.max[u]);
        for (int32_t i = shape_face_cell(b.min[u]); i < end; i++) {
            if ((f.rows[i] & mask) != mask) return false;
        }
    }
    return true;
}

bool shape_is_block(const shape & s) {
    std::vector<uint32_t> columns(32 * 32, 0);  // z cells as bits, per x and y cell
    for (const aabb & b : s.boxes) {
        for (int32_t axis = 0; axis < 3; axis++) {
            if (b.min[axis] < 0.0 || b.max[axis] > 1.0) return false;
        }
        uint32_t mask = shape_face_mask(b.min[2], b.max[2]);
        for (int32_t x = shape_face_cell(b.min[0]); x < shape_face_cell(b.max[0]); x++) {
            for (int32_t y = shape_face_cell(b.min[1]); y < shape_face_cell(b.max[1]); y++) columns[x * 32 + y] |= mask;
        }
    }
    for (uint32_t column : columns) {
        if (column != UINT32_MAX) return false;
    }
    return true;
}

// the slice where the shape ends along axis; empty unless that end is the block face (fuzzy, like VoxelShape.max)
static void shape_add_end_slice(uint32_t rows[32], const shape & s, int32_t axis, bool max_end) {
    double face = max_end ? 1.0 : 0.0;
    double end  = max_end ? -INFINITY : INFINITY;
    for (const aabb & b : s.boxes) end = max_end ? std::max(end, b.max[axis]) : std::min(end, b.min[axis]);
    if (std::fabs(end - face) > 1.0E-7) return;
    int32_t u;
    int32_t v;
    shape_face_axes(axis, u, v);
    for (const aabb & b : s.boxes) {
        if (std::fabs((max_end ? b.max[axis] : b.min[axis]) - face) > 1.0E-7) continue;
        uint32_t mask = shape_face_mask(b.min[v], b.max[v]);
        int32_t  last = shape_face_cell(b.max[u]);
        for (int32_t i = shape_face_cell(b.min[u]); i < last; i++) rows[i] |= mask;
    }
}

bool shape_merged_face_occludes(const shape & s, const shape & occluder, direction dir) {
    const block_pos & step     = DIRECTION_STEPS[dir];
    bool              positive = step.x + step.y + step.z > 0;
    uint32_t          rows[32] = {};
    shape_add_end_slice(rows, positive ? s : occluder, direction_get_axis(dir), true);
    shape_add_end_slice(rows, positive ? occluder : s, direction_get_axis(dir), false);
    for (uint32_t row : rows) {
        if (row != UINT32_MAX) return false;
    }
    return true;
}

static double voxel_shape_get(const voxel_shape & s, int32_t axis, int32_t i) {
    return voxel_coords[voxel_defs[s.id].coords_first[axis] + i] + s.offset[axis];
}

// DiscreteVoxelShape.isFullWide: false outside the cells
static bool voxel_def_is_full_wide(const voxel_def & d, const int32_t cell[3]) {
    for (int32_t axis = 0; axis < 3; axis++) {
        if (cell[axis] < 0 || cell[axis] >= d.size[axis]) return false;
    }
    return voxel_cells[d.cells_first + (cell[0] * d.size[1] + cell[1]) * d.size[2] + cell[2]] != 0;
}

bool voxel_shape_is_empty(const voxel_shape & s) {
    const voxel_def & d = voxel_defs[s.id];
    for (int32_t i = 0; i < d.size[0] * d.size[1] * d.size[2]; i++) {
        if (voxel_cells[d.cells_first + i] != 0) return false;
    }
    return true;
}

double voxel_shape_max(const voxel_shape & s, int32_t axis) {
    const voxel_def & d    = voxel_defs[s.id];
    int32_t           last = -1;
    for (int32_t x = 0; x < d.size[0]; x++) {
        for (int32_t y = 0; y < d.size[1]; y++) {
            for (int32_t z = 0; z < d.size[2]; z++) {
                const int32_t at[3] = { x, y, z };
                if (voxel_cells[d.cells_first + (x * d.size[1] + y) * d.size[2] + z] != 0) {
                    last = std::max(last, at[axis]);
                }
            }
        }
    }
    if (last < 0) return -INFINITY;
    return voxel_coords[d.coords_first[axis] + last + 1] + s.offset[axis];
}

// VoxelShape.findIndex of a moved shape: Mth.binarySearch for the first coordinate above coord, minus one
static int32_t voxel_shape_find_index(const voxel_shape & s, int32_t axis, double coord) {
    int32_t from = 0;
    int32_t len  = voxel_defs[s.id].size[axis] + 1;
    while (len > 0) {
        int32_t half = len / 2;
        if (coord < voxel_shape_get(s, axis, from + half)) {
            len = half;
        } else {
            from += half + 1;
            len -= half + 1;
        }
    }
    return from - 1;
}

// VoxelShape.collideX with the axes cycled so that a is the moving axis; the first full layer in the way decides
double voxel_shape_collide(const voxel_shape & s, int32_t a, const aabb & moving, double distance) {
    if (voxel_shape_is_empty(s)) return distance;
    if (std::fabs(distance) < 1.0E-7) return 0.0;
    const voxel_def & d     = voxel_defs[s.id];
    int32_t           b     = (a + 1) % 3;
    int32_t           c     = (a + 2) % 3;
    int32_t           a_min = voxel_shape_find_index(s, a, moving.min[a] + 1.0E-7);
    int32_t           a_max = voxel_shape_find_index(s, a, moving.max[a] - 1.0E-7);
    int32_t           b_min = std::max(0, voxel_shape_find_index(s, b, moving.min[b] + 1.0E-7));
    int32_t           b_max = std::min(d.size[b], voxel_shape_find_index(s, b, moving.max[b] - 1.0E-7) + 1);
    int32_t           c_min = std::max(0, voxel_shape_find_index(s, c, moving.min[c] + 1.0E-7));
    int32_t           c_max = std::min(d.size[c], voxel_shape_find_index(s, c, moving.max[c] - 1.0E-7) + 1);
    bool              up    = distance > 0.0;
    for (int32_t i = up ? a_max + 1 : a_min - 1; up ? i < d.size[a] : i >= 0; i += up ? 1 : -1) {
        for (int32_t j = b_min; j < b_max; j++) {
            for (int32_t k = c_min; k < c_max; k++) {
                int32_t cell[3];
                cell[a] = i;
                cell[b] = j;
                cell[c] = k;
                if (!voxel_def_is_full_wide(d, cell)) continue;
                if (up) {
                    double gap = voxel_shape_get(s, a, i) - moving.max[a];
                    if (gap >= -1.0E-7) distance = std::min(distance, gap);
                } else {
                    double gap = voxel_shape_get(s, a, i + 1) - moving.min[a];
                    if (gap <= 1.0E-7) distance = std::max(distance, gap);
                }
                return distance;
            }
        }
    }
    return distance;
}

// VoxelShape.min and max along an axis: the bounds of the full cells
static void voxel_shape_bounds(const voxel_shape & s, int32_t axis, double & lo, double & hi) {
    const voxel_def & d     = voxel_defs[s.id];
    int32_t           first = d.size[axis];
    int32_t           last  = 0;
    int32_t           cell[3];
    for (cell[0] = 0; cell[0] < d.size[0]; cell[0]++) {
        for (cell[1] = 0; cell[1] < d.size[1]; cell[1]++) {
            for (cell[2] = 0; cell[2] < d.size[2]; cell[2]++) {
                if (!voxel_def_is_full_wide(d, cell)) continue;
                first = std::min(first, cell[axis]);
                last  = std::max(last, cell[axis] + 1);
            }
        }
    }
    lo = first >= d.size[axis] ? INFINITY : voxel_shape_get(s, axis, first);
    hi = last <= 0 ? -INFINITY : voxel_shape_get(s, axis, last);
}

// IndirectMerger with the box along one axis, for AND: the shape cells that pair with the box's one cell
static std::vector<int32_t> voxel_shape_merge_with_box(const voxel_shape & s, int32_t axis, double lo, double hi) {
    int32_t              first_size  = voxel_defs[s.id].size[axis] + 1;
    double               box[2]      = { lo, hi };
    double               last_value  = NAN;
    int32_t              first_index = 0;
    int32_t              box_index   = 0;
    std::vector<int32_t> firsts;
    std::vector<int32_t> boxes;
    while (first_index < first_size || box_index < 2) {
        bool ran_out_of_first = first_index >= first_size;
        bool ran_out_of_box   = box_index >= 2;
        bool chose_first =
            !ran_out_of_first && (ran_out_of_box || voxel_shape_get(s, axis, first_index) < box[box_index] + 1.0E-7);
        if (chose_first) {
            first_index++;
            if (box_index == 0 || ran_out_of_box) continue;
        } else {
            box_index++;
            if (first_index == 0 || ran_out_of_first) continue;
        }
        double next = chose_first ? voxel_shape_get(s, axis, first_index - 1) : box[box_index - 1];
        if (!(last_value >= next - 1.0E-7)) {
            firsts.push_back(first_index - 1);
            boxes.push_back(box_index - 1);
            last_value = next;
        } else {
            firsts.back() = first_index - 1;
            boxes.back()  = box_index - 1;
        }
    }
    // the merged list has one more value than cells; only cells where the box is full matter
    std::vector<int32_t> cells;
    for (size_t i = 0; i + 1 < firsts.size(); i++) {
        if (boxes[i] == 0) cells.push_back(firsts[i]);
    }
    return cells;
}

bool voxel_shape_join_is_not_empty(const voxel_shape & s, const aabb & box) {
    for (int32_t axis = 0; axis < 3; axis++) {
        if (box.max[axis] - box.min[axis] < 1.0E-7) return false;
    }
    if (voxel_shape_is_empty(s)) return false;
    for (int32_t axis = 0; axis < 3; axis++) {
        double lo;
        double hi;
        voxel_shape_bounds(s, axis, lo, hi);
        if (hi < box.min[axis] - 1.0E-7 || box.max[axis] < lo - 1.0E-7) return false;
    }
    std::vector<int32_t> cells[3];
    for (int32_t axis = 0; axis < 3; axis++) {
        int32_t last = voxel_defs[s.id].size[axis];
        // createIndexMerger: lists that do not overlap pair every cell with nothing
        if (voxel_shape_get(s, axis, last) < box.min[axis] - 1.0E-7 ||
            box.max[axis] < voxel_shape_get(s, axis, 0) - 1.0E-7) {
            return false;
        }
        cells[axis] = voxel_shape_merge_with_box(s, axis, box.min[axis], box.max[axis]);
    }
    const voxel_def & d = voxel_defs[s.id];
    for (int32_t x : cells[0]) {
        for (int32_t y : cells[1]) {
            for (int32_t z : cells[2]) {
                int32_t cell[3] = { x, y, z };
                if (voxel_def_is_full_wide(d, cell)) return true;
            }
        }
    }
    return false;
}
