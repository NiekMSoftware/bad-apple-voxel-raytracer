#include "rt/scene/brick_pool.h"

namespace rt::scene {

    BrickPool::BrickPool()
    {
        m_pMap = static_cast<BrickMap*>(MALLOC64(sizeof(BrickMap)));
        memset(m_pMap, 0, sizeof(BrickMap));
        m_pMap->m_brickCount = 1;  // index 0 is the null/empty sentinel

        // Allocate the initial brick pool separately so it can be
        // grown independently of the fixed-size grid array.
        m_pMap->m_brickCapacity = INITIAL_BRICK_CAPACITY;
        m_pMap->m_pBricks = static_cast<Brick*>(MALLOC64(
            static_cast<size_t>(m_pMap->m_brickCapacity) * sizeof(Brick)));
        memset(m_pMap->m_pBricks, 0, static_cast<size_t>(m_pMap->m_brickCapacity) * sizeof(Brick));

        printf("BrickPool initialized: grid = %u^3 (%u cells), "
               "brick pool = %u bricks (%.1f MB)\n",
               GRIDSIZE, GRIDSIZE3,
               m_pMap->m_brickCapacity,
               static_cast<float>(m_pMap->m_brickCapacity) * sizeof(Brick) / (1024.0f * 1024.0f));
    }

    BrickPool::~BrickPool()
    {
        if (m_pMap) {
            printf("BrickPool destroyed: %u / %u bricks used (%.1f%%), "
                   "peak memory: %.1f MB\n",
                   m_pMap->m_brickCount, m_pMap->m_brickCapacity,
                   100.0f * m_pMap->m_brickCount / m_pMap->m_brickCapacity,
                   static_cast<float>(m_pMap->m_brickCapacity) * sizeof(Brick) / (1024.0f * 1024.0f));

            if (m_pMap->m_pBricks) { FREE64(m_pMap->m_pBricks); m_pMap->m_pBricks = nullptr; }
            FREE64(m_pMap);
            m_pMap = nullptr;
        }
    }

    void BrickPool::growBrickPool() const
    {
        const uint32_t oldCapacity = m_pMap->m_brickCapacity;
        const uint32_t newCapacity = m_pMap->m_brickCapacity * 2;

        // Safety clamp: never exceed the theoretical max of GRIDSIZE3
        const uint32_t clampedCapacity = (newCapacity > GRIDSIZE3) ? GRIDSIZE3 : newCapacity;
        if (clampedCapacity <= m_pMap->m_brickCount) return;

        auto* newBricks = static_cast<Brick*>(MALLOC64(
            static_cast<size_t>(clampedCapacity) * sizeof(Brick)));
        if (!newBricks) return;

        // Copy existing bricks into the new pool
        memcpy(newBricks, m_pMap->m_pBricks,
               static_cast<size_t>(m_pMap->m_brickCount) * sizeof(Brick));

        // Zero-initialize the newly added region
        memset(newBricks + m_pMap->m_brickCount, 0,
               static_cast<size_t>(clampedCapacity - m_pMap->m_brickCount) * sizeof(Brick));

        FREE64(m_pMap->m_pBricks);
        m_pMap->m_pBricks        = newBricks;
        m_pMap->m_brickCapacity = clampedCapacity;

        printf("BrickPool grew: %u -> %u bricks (%.1f MB -> %.1f MB), "
               "currently used: %u (%.1f%%)\n",
               oldCapacity, m_pMap->m_brickCapacity,
               static_cast<float>(oldCapacity)        * sizeof(Brick) / (1024.0f * 1024.0f),
               static_cast<float>(m_pMap->m_brickCapacity) * sizeof(Brick) / (1024.0f * 1024.0f),
               m_pMap->m_brickCount,
               100.0f * m_pMap->m_brickCount / m_pMap->m_brickCapacity);
    }

    void BrickPool::set(const uint x, const uint y, const uint z, const uint v) const {
        const uint bx = x / BRICKSIZE, by = y / BRICKSIZE, bz = z / BRICKSIZE;
        const uint cellIdx = bx + by * GRIDSIZE + bz * GRIDSIZE2;
        uint& gridVal = m_pMap->m_aGrid[cellIdx];

        if (!(gridVal & 1)) {
            // Grow the brick pool if we've run out of space
            if (m_pMap->m_brickCount >= m_pMap->m_brickCapacity) growBrickPool();
            // If still full after trying to grow, bail out
            if (m_pMap->m_brickCount >= m_pMap->m_brickCapacity) return;

            const uint bi = m_pMap->m_brickCount++;
            gridVal = (bi << 1) | 1;  // store brick index + occupied flag
            m_pMap->setOccupied(cellIdx);

            // Update super-grid occupancy:
            // Each super-cell covers 8x8x8 outer grid cells.
            const uint sx = bx / 8, sy = by / 8, sz = bz / 8;
            const uint superIdx = sx + sy * SUPERGRIDSIZE + sz * SUPERGRIDSIZE2;
            m_pMap->setSuperOccupied(superIdx);

            // Update Y-slice occupancy
            m_pMap->setYSliceOccupied(by);
        }

        const uint bi  = gridVal >> 1;
        const uint lx  = x % BRICKSIZE, ly = y % BRICKSIZE, lz = z % BRICKSIZE;
        m_pMap->m_pBricks[bi].m_aVoxel[lx + ly * BRICKSIZE + lz * BRICKSIZE2] = v;
    }

    uint32_t BrickPool::voxelAt(const uint x, const uint y, const uint z) const
    {
        const uint bx = x / BRICKSIZE, by = y / BRICKSIZE, bz = z / BRICKSIZE;
        const uint gridVal = m_pMap->m_aGrid[bx + by * GRIDSIZE + bz * GRIDSIZE2];
        if (!(gridVal & 1)) return 0;  // cell has no brick

        const uint bi = gridVal >> 1;
        const uint lx = x % BRICKSIZE, ly = y % BRICKSIZE, lz = z % BRICKSIZE;
        return m_pMap->m_pBricks[bi].m_aVoxel[lx + ly * BRICKSIZE + lz * BRICKSIZE2];
    }

}  // namespace Tmpl8