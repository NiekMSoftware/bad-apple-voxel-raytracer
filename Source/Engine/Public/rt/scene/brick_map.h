#pragma once
#include <cstdint>

// ============================================================
// High level settings
// ============================================================
#define WORLDSIZE 512   // Must be a power of 2. Max 512.

// Derived world constants
#define WORLDSIZE2  (WORLDSIZE * WORLDSIZE)
#define WORLDSIZE3  (WORLDSIZE * WORLDSIZE * WORLDSIZE)

// Brick dimensions - each brick is an 8x8x8 block of voxels.
// Chosen as 8 so that GRIDSIZE = WORLDSIZE/8, which is small
// enough to traverse quickly at the outer DDA level.
#define BRICKSIZE   8
#define BRICKSIZE2  (BRICKSIZE * BRICKSIZE)
#define BRICKSIZE3  (BRICKSIZE * BRICKSIZE * BRICKSIZE)

// Top-level grid dimensions
#define GRIDSIZE    (WORLDSIZE / BRICKSIZE)
#define GRIDSIZE2   (GRIDSIZE * GRIDSIZE)
#define GRIDSIZE3   (GRIDSIZE * GRIDSIZE * GRIDSIZE)

#define CELLSIZE              (1.0f / GRIDSIZE)
#define VOXELSIZE             (1.0f / WORLDSIZE)
#define INITIAL_BRICK_CAPACITY (GRIDSIZE3 / 16)

#define EPSILON     0.00001f

namespace rt::scene {

    // ============================================================
    // Brick
    // Stores 8x8x8 = 512 voxels as packed uint32_t values.
    //
    // Each uint32_t matches the layout in bit_packer.h:
    //   bits [31..24] = material ID
    //   bits [23..16] = R
    //   bits [15..8 ] = G
    //   bits [ 7..0 ] = B
    // ============================================================
    struct Brick
    {
        uint32_t m_aVoxel[BRICKSIZE3];   // 2048 bytes per brick
    };

    // ============================================================
    // BrickMap
    // Two-level sparse grid. Pure data — no allocation logic lives
    // here; that is owned by BrickPool.
    //
    //   grid[] entry layout:
    //     bit 0      = 1 if a brick is allocated for this cell, else 0
    //     bits[31:1] = brick index into bricks[]
    //
    //   The brick pool is dynamically allocated and grows as needed.
    //   Initial capacity is a fraction of GRIDSIZE3; doubled on overflow.
    // ============================================================
    struct BrickMap
    {
        uint32_t m_aGrid[GRIDSIZE3]{};   // top-level coarse grid
        Brick*   m_pBricks       = nullptr; // dynamically allocated brick pool
        uint32_t m_brickCount    = 1;   // index 0 reserved as "null/empty" sentinel
        uint32_t m_brickCapacity = 0;   // current allocated capacity

        uint32_t m_aOccupancy[GRIDSIZE3 / 32 + 1]{};

        void setOccupied(const uint32_t idx)
        {
            m_aOccupancy[idx >> 5] |= (1u << (idx & 31));
        }

        bool isOccupied(const uint32_t idx) const
        {
            return (m_aOccupancy[idx >> 5] >> (idx & 31)) & 1u;
        }
    };

}  // namespace Tmpl8