#pragma once
#include "rt/core/ray.h"
#include "rt/scene/brick_map.h"
#include "dda.h"

namespace rt::rendering {
    class MaterialManager;
}

// ============================================================
// BrickTraverser
// Stateless free functions for all brick-level ray queries.
//
// These functions have no ownership — they receive a const
// BrickMap pointer and a Ray, and return results through
// output parameters or the Ray itself. All three public ray
// query methods on Scene delegate to functions here.
//
// Three query variants exist, matching the three traversal
// modes used by the renderer:
//   Nearest      — finds the closest solid voxel hit
//   Occluded     — early-exits on any solid voxel (shadow test)
//   Transmittance — accumulates Beer-Lambert absorption through
//                   transparent voxels
// ============================================================
namespace rt::scene::internal {

    // Decodes the grid entry at outer cell (X, Y, Z) and fills
    // brickIdx, brickWorldSize, and brickOrigin.
    // Returns false if the cell is empty (no brick allocated).
    inline bool GetBrickInfo(const BrickMap* map,
                            const uint X, const uint Y, const uint Z,
                            uint& brickIdx,
                            float3& brickOrigin)
    {
        const uint cellIdx = X + Y * GRIDSIZE + Z * GRIDSIZE2;

        // Fast reject from compact bitfield
        if (!map->isOccupied(cellIdx)) return false;
        
        brickIdx       = map->m_aGrid[cellIdx] >> 1;
        brickOrigin    = make_float3(
            static_cast<float>(X),
            static_cast<float>(Y),
            static_cast<float>(Z)) * CELLSIZE;
        return true;
    }

    // Initializes a DDAState to step through the GRIDSIZE^3 outer grid.
    // Computes the grid entry cell and tmax/tdelta for the first step.
    // Also sets ray.inside if the ray origin lies inside a solid voxel.
    // Returns false if the ray misses the world cube entirely.
    bool Setup3DDDA(const BrickMap* map, core::Ray& ray, DDAState& state);

    // Overload: initialise the fine DDA starting at a caller-supplied t.
    // Used by the super-grid accelerated paths so the fine DDA begins
    // at the super-cell entry point instead of the ray origin.
    // Does NOT perform the inside-solid check (caller handles that).
    bool Setup3DDDAAt(const BrickMap* map, const core::Ray& ray,
                      DDAState& state, float entryT);

    // Nearest-hit traversal inside one brick using the inner DDA.
    //
    // Normal mode (insideMode = false):
    //   Stops on the first solid voxel encountered. Writes ray.voxel,
    //   ray.t, ray.axis and returns true on hit; returns false on miss.
    //
    // Inside mode (insideMode = true):
    //   Ray started inside solid — walks forward until it finds the
    //   first empty voxel (the exit surface). Returns true on exit,
    //   false if the ray exits the brick without finding empty space
    //   (outer loop must continue into the next brick).
    bool Nearest(core::Ray& ray, const BrickMap* map,
                 uint brickIdx, float3 brickOrigin,
                 float entryT, uint& axis,
                 bool insideMode = false);

    // Shadow variant — returns true immediately on any solid voxel hit.
    // Does not write to ray. Respects ray.t as the maximum distance.
    bool Occluded(const core::Ray& ray, const BrickMap* map,
                  uint brickIdx, float3 brickOrigin, float entryT);

    // Transmittance variant — accumulates Beer-Lambert absorption for
    // each transparent voxel segment along the ray.
    // Returns false if transmittance drops to effectively zero (fully
    // blocked). Returns true with the updated transmittance otherwise.
    bool Transmittance(const core::Ray& ray, const BrickMap* map,
                       uint brickIdx, float3 brickOrigin,
                       float entryT,
                       const rt::rendering::MaterialManager& materials,
                       float3& transmittance);

}  // namespace Tmpl8::BrickTraverser