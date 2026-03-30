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

// Super-grid: groups of 8x8x8 outer cells for coarse space-skipping.
// The DDA can step through this 8^3 grid first and only descend into
// the fine 64^3 grid when a super-cell is occupied.
#define SUPERGRIDSIZE   (GRIDSIZE / 8)          // = 8
#define SUPERGRIDSIZE2  (SUPERGRIDSIZE * SUPERGRIDSIZE)
#define SUPERGRIDSIZE3  (SUPERGRIDSIZE * SUPERGRIDSIZE * SUPERGRIDSIZE)
#define SUPERCELLSIZE   (1.0f / SUPERGRIDSIZE)

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

        // ============================================================
        // Super-grid occupancy — 8^3 = 512 cells, 1 bit each.
        // Each super-cell covers 8x8x8 outer grid cells.
        // Allows the DDA to skip large empty regions in ~8 steps
        // instead of ~64.
        //
        // Memory: 512 / 8 = 64 bytes — fits in a single cache line.
        // ============================================================
        uint32_t m_aSuperOccupancy[SUPERGRIDSIZE3 / 32 + 1]{};

        // ============================================================
        // Y-slice occupancy — 1 bit per Y-level of the outer grid.
        // Bit y is set if ANY outer cell at row y is occupied.
        //
        // Shadow rays going upward from the floor (Y=0) can check
        // this single 64-bit value to skip the entire world DDA
        // when all Y-slices above the ray origin are empty.
        //
        // Memory: 8 bytes.
        // ============================================================
        uint64_t m_ySliceOccupancy = 0;

        // --- Fine-grid occupancy (existing) ---
        void setOccupied(const uint32_t idx)
        {
            m_aOccupancy[idx >> 5] |= (1u << (idx & 31));
        }

        bool isOccupied(const uint32_t idx) const
        {
            return (m_aOccupancy[idx >> 5] >> (idx & 31)) & 1u;
        }

        // --- Super-grid occupancy ---
        void setSuperOccupied(const uint32_t superIdx)
        {
            m_aSuperOccupancy[superIdx >> 5] |= (1u << (superIdx & 31));
        }

        bool isSuperOccupied(const uint32_t superIdx) const
        {
            return (m_aSuperOccupancy[superIdx >> 5] >> (superIdx & 31)) & 1u;
        }

        // --- Y-slice occupancy ---
        void setYSliceOccupied(const uint32_t y)
        {
            m_ySliceOccupancy |= (1ULL << y);
        }

        bool isYSliceEmpty(const uint32_t y) const
        {
            return !(m_ySliceOccupancy & (1ULL << y));
        }

        // Returns true if ALL Y-slices strictly above 'y' are empty.
        // Used by shadow rays heading upward to skip the entire world DDA.
        bool allYSlicesAboveEmpty(const uint32_t y) const
        {
            if (y >= GRIDSIZE - 1) return true;        // already at top
            const uint64_t aboveMask = ~((1ULL << (y + 1)) - 1);
            return (m_ySliceOccupancy & aboveMask) == 0;
        }

        // Returns true if ALL Y-slices strictly below 'y' are empty.
        // Used by shadow rays heading downward.
        bool allYSlicesBelowEmpty(const uint32_t y) const
        {
            if (y == 0) return true;
            const uint64_t belowMask = (1ULL << y) - 1;
            return (m_ySliceOccupancy & belowMask) == 0;
        }
    };

}  // namespace Tmpl8