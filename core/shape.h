#pragma once

#include "core/aabb.h"
#include "core/direction.h"
#include "core/state.h"

#include <cstdint>
#include <vector>

// A collision shape as the union of its boxes (VoxelShape.toAabbs).
// Every bound lies on a 1/32 grid, even when a piston moves it half a block, so a face is exact as a 32 x 32 raster.
struct shape {
    std::vector<aabb> boxes;
};

// VoxelShape.getFaceShape: the covered 1/32 cells, row by the first other axis and bit by the second
struct shape_face {
    int32_t  axis;
    uint32_t rows[32];
    bool     outside;  // the shape reaches past the block face
};

shape shape_block();
shape shape_from_table(uint16_t id);
// BlockBehaviour.Cache.collisionShape of a state without a dynamic shape
shape shape_collision(state_id st);
shape shape_move(shape s, double dx, double dy, double dz);

bool shape_is_block(const shape & s);

shape_face shape_get_face(const shape & s, direction dir);
// Block.isFaceFull
bool       shape_face_is_full(const shape_face & f);
// the shape lies inside the face shape, which spans the whole block along its axis (WallBlock.isCovered)
bool       shape_face_covers(const shape_face & f, uint16_t shape_id);

// Shapes.mergedFaceOccludes when neither shape is Shapes.block()
bool shape_merged_face_occludes(const shape & s, const shape & occluder, direction dir);

// A dumped shape moved by offset as BlockCollisions returns it (OffsetDoubleList), so collisions round like vanilla's
struct voxel_shape {
    uint16_t id;
    double   offset[3];
};

bool   voxel_shape_is_empty(const voxel_shape & s);
// VoxelShape.max: the far bound of the last full cell along axis, -infinity for an empty shape
double voxel_shape_max(const voxel_shape & s, int32_t axis);
// VoxelShape.collide
double voxel_shape_collide(const voxel_shape & s, int32_t axis, const aabb & moving, double distance);
// Shapes.joinIsNotEmpty(s, Shapes.create(box), AND) for a box that Shapes.create keeps as one cell (not in the unit cube)
bool   voxel_shape_join_is_not_empty(const voxel_shape & s, const aabb & box);
