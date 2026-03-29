#pragma once

#include "rt/scene/brick_map.h"
#include "tmpl8/template.h"

namespace rt::scene {

    // ============================================================
    // BrickPool
    // Owns the BrickMap heap allocation and all voxel read/write
    // operations. Separating this from Scene keeps memory management
    // in one place and away from ray traversal logic.
    //
    // GrowBrickPool is called automatically by Set when the pool
    // is full — callers never need to trigger it manually.
    //
    // The raw BrickMap pointer is exposed read-only to BrickTraverser
    // so traversal can access voxel data without going through the
    // write path.
    // ============================================================
    class BrickPool
    {
    public:
        BrickPool();
        ~BrickPool();

        // Disable copy — pool owns aligned heap memory
        BrickPool(const BrickPool&)            = delete;
        BrickPool& operator=(const BrickPool&) = delete;

        // Write a voxel value v at world-integer coordinates (x, y, z).
        // Allocates a new brick for the cell if one does not exist yet.
        void set(uint x, uint y, uint z, uint v) const;

        // Returns the packed voxel uint32 at world-integer coords,
        // or 0 if the cell has no brick or the voxel is empty.
        [[nodiscard]] uint32_t voxelAt(uint x, uint y, uint z) const;

        // Raw map access for DDA traversal — read-only outside this class.
        [[nodiscard]] const BrickMap* getMap() const { return m_pMap; }
                            BrickMap* getMap()       { return m_pMap; }

    private:
        // Doubles the brick pool capacity when it fills up.
        // Safety-clamped to GRIDSIZE3 — the theoretical maximum.
        void growBrickPool() const;

        BrickMap* m_pMap = nullptr;
    };

}  // namespace Tmpl8