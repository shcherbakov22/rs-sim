#pragma once

#include "core/direction.h"
#include "core/state.h"

#include <vector>

// VoxelShape: full or empty cells between bounds on each axis. Shapes are kept trimmed and without bounds that
// separate equal cells, as optimize() leaves them; vanilla's representation then gives the same face answers.
struct voxel_shape {
    std::vector<double>  coords[3];  // per axis x y z; all empty for the empty shape
    std::vector<uint8_t> full;       // cell (x * ny + y) * nz + z
};

// BooleanOp
enum boolean_op {
    BOOLEAN_OP_OR,
    BOOLEAN_OP_ONLY_FIRST,
    BOOLEAN_OP_ONLY_SECOND,
    BOOLEAN_OP_NOT_SAME,
};

// Shapes
voxel_shape shapes_block();
voxel_shape shapes_box(double x1, double y1, double z1, double x2, double y2, double z2);
voxel_shape shapes_join(const voxel_shape & a, const voxel_shape & b, boolean_op op);
bool        shapes_join_is_not_empty(const voxel_shape & a, const voxel_shape & b, boolean_op op);

// VoxelShape
bool        voxel_shape_is_empty(const voxel_shape & s);
voxel_shape voxel_shape_move(const voxel_shape & s, double dx, double dy, double dz);
voxel_shape voxel_shape_get_face_shape(const voxel_shape & s, direction dir);

// a generated shape (shape_defs)
voxel_shape voxel_shape_from_table(uint16_t id);
// BlockBehaviour.Cache.collisionShape of a state without a dynamic shape
voxel_shape voxel_shape_collision(state_id st);
