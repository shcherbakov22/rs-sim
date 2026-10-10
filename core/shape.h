#pragma once

#include "core/direction.h"
#include "core/state.h"

#include <cstdint>
#include <vector>

// A collision shape as the union of its boxes (VoxelShape.toAabbs). Every bound of a block shape, also when a piston
// moves it by half a block, lies on a 1/32 grid, so a face is exact as a 32 x 32 raster.
struct shape {
    std::vector<shape_box> boxes;
};

// VoxelShape.getFaceShape seen along the face's axis: the 1/32 cells of the block face it covers, row by the first
// other axis and bit by the second, and whether it reaches past the block face
struct shape_face {
    int32_t  axis;
    uint32_t rows[32];
    bool     outside;
};

// Block.box, in 1/16 block units
shape_box block_box(double x1, double y1, double z1, double x2, double y2, double z2);

shape shape_block();
shape shape_from_table(uint16_t id);
// BlockBehaviour.Cache.collisionShape of a state without a dynamic shape
shape shape_collision(state_id st);
shape shape_move(shape s, double dx, double dy, double dz);

shape_face shape_get_face(const shape & s, direction dir);
// Block.isFaceFull
bool       shape_face_is_full(const shape_face & f);
// the box lies inside the face shape, which spans the whole block along its axis (WallBlock.isCovered)
bool       shape_face_covers(const shape_face & f, const shape_box & b);
