#include "brick_traverser.h"
#include "cube_helpers.h"
#include "dda.h"

#include "tmpl8/template.h"
#include "rt/rendering/bit_packer.h"
#include "rt/rendering/material_manager.h"

namespace rt::scene::internal {

    // bool GetBrickInfo(const BrickMap* map,
    //                   const uint X, const uint Y, const uint Z,
    //                   uint& brickIdx, float& brickWorldSize, float3& brickOrigin)
    // {
    //     const uint gridVal = map->m_aGrid[X + Y * GRIDSIZE + Z * GRIDSIZE2];
    //     if (!(gridVal & 1)) return false;  // cell is empty, no brick here
    //
    //     brickIdx       = gridVal >> 1;
    //     brickWorldSize = 1.0f / GRIDSIZE;
    //     brickOrigin    = make_float3(
    //         static_cast<float>(X),
    //         static_cast<float>(Y),
    //         static_cast<float>(Z)) * brickWorldSize;
    //     return true;
    // }

    bool Setup3DDDA(const BrickMap* map, core::Ray& ray, DDAState& state)
    {
        state.m_t = 0.f;

        const bool startedInGrid = pointInCube(ray.m_o);
        if (!startedInGrid) {
            // Ray originates outside the unit cube — intersect it first
            state.m_t = intersectCube(ray);
            if (state.m_t > 1e33f) return false;  // ray misses the world entirely
        }

        state.m_step = make_int3(1.0f - ray.m_dsign * 2.0f);

        // Nudge avoids landing exactly on a grid boundary
        const float3 posInGrid  = static_cast<float>(GRIDSIZE) * (ray.m_o + (state.m_t + 0.00005f) * ray.m_d);
        const float3 gridPlanes = (ceilf(posInGrid) - ray.m_dsign) * CELLSIZE;
        const int3   P          = clamp(make_int3(posInGrid), 0, GRIDSIZE - 1);

        state.m_x      = P.x; state.m_y = P.y; state.m_z = P.z;
        state.m_tDelta = CELLSIZE * float3(state.m_step) * ray.m_rD;
        state.m_tmax   = (gridPlanes - ray.m_o) * ray.m_rD;
        state.m_axis   = ray.m_axis;

        if (startedInGrid) {
            // Check whether the ray origin sits inside a solid voxel
            const uint wx = min(static_cast<uint>(ray.m_o.x * WORLDSIZE), static_cast<uint>(WORLDSIZE - 1));
            const uint wy = min(static_cast<uint>(ray.m_o.y * WORLDSIZE), static_cast<uint>(WORLDSIZE - 1));
            const uint wz = min(static_cast<uint>(ray.m_o.z * WORLDSIZE), static_cast<uint>(WORLDSIZE - 1));
            const uint bx = wx / BRICKSIZE, by = wy / BRICKSIZE, bz = wz / BRICKSIZE;
            const uint gridVal = map->m_aGrid[bx + by * GRIDSIZE + bz * GRIDSIZE2];
            if (gridVal & 1) {
                const uint bi = gridVal >> 1;
                const uint lx = wx % BRICKSIZE, ly = wy % BRICKSIZE, lz = wz % BRICKSIZE;
                ray.m_bInside = map->m_pBricks[bi].m_aVoxel[lx + ly * BRICKSIZE + lz * BRICKSIZE2] != 0;
            } else {
                ray.m_bInside = false;
            }
        } else {
            ray.m_bInside = false;
        }

        return true;
    }

    // --------------------------------------------------------
    // Setup3DDDAAt — initialise the fine DDA at an arbitrary t
    //
    // Identical to Setup3DDDA except:
    //   - entryT is supplied by the caller (from the super-grid
    //     DDA) instead of being computed from a cube intersection.
    //   - No inside-solid check — the super-grid path handles
    //     the inside case separately before calling this.
    //   - The ray is taken by const-ref since we don't write
    //     m_bInside.
    //
    // The entry point (ray.m_o + entryT * ray.m_d) must lie
    // within or on the boundary of the unit cube [0,1]^3.
    // --------------------------------------------------------
    bool Setup3DDDAAt(const BrickMap* /*map*/, const core::Ray& ray,
                      DDAState& state, const float entryT)
    {
        state.m_t    = entryT;
        state.m_step = make_int3(1.0f - ray.m_dsign * 2.0f);

        // Compute the grid position at the entry point (not the ray origin)
        const float3 posInGrid  = static_cast<float>(GRIDSIZE) * (ray.m_o + (entryT + 0.00005f) * ray.m_d);
        const float3 gridPlanes = (ceilf(posInGrid) - ray.m_dsign) * CELLSIZE;
        const int3   P          = clamp(make_int3(posInGrid), 0, GRIDSIZE - 1);

        state.m_x      = P.x; state.m_y = P.y; state.m_z = P.z;
        state.m_tDelta = CELLSIZE * float3(state.m_step) * ray.m_rD;
        state.m_tmax   = (gridPlanes - ray.m_o) * ray.m_rD;
        state.m_axis   = ray.m_axis;

        return true;
    }

    bool Nearest(core::Ray& ray, const BrickMap* map,
                 const uint brickIdx, const float3 brickOrigin,
                 const float entryT, uint& axis, const bool insideMode)
    {
        InnerDDAState s = setupInnerDda(ray, brickOrigin, entryT, axis);
        const uint32_t* voxels = map->m_pBricks[brickIdx].m_aVoxel;

        if (!insideMode) {
            // Normal mode: stop on the first solid voxel
            do {
                const uint32_t v = voxels[s.m_x + s.m_y * BRICKSIZE + s.m_z * BRICKSIZE2];
                if (v) {
                    ray.m_voxel = v;
                    ray.m_t     = s.m_t;
                    ray.m_axis  = s.m_axis;
                    axis      = s.m_axis;
                    return true;
                }
            } while (advanceInnerDda(s));

            return false;
        }

        // Inside mode: ray started inside solid — walk until we find empty space
        uint  lastVoxel = voxels[s.m_x + s.m_y * BRICKSIZE + s.m_z * BRICKSIZE2];
        float lastT     = s.m_t;
        uint  lastAxis  = s.m_axis;

        while (advanceInnerDda(s)) {
            const uint32_t v = voxels[s.m_x + s.m_y * BRICKSIZE + s.m_z * BRICKSIZE2];
            if (!v) {
                // The step that just happened crossed from solid into empty.
                // s.t and s.axis are the crossing point — use them for the exit surface.
                ray.m_voxel = lastVoxel;
                ray.m_t     = s.m_t;
                ray.m_axis  = s.m_axis;
                axis      = s.m_axis;
                return true;
            }

            // Still inside solid: record this voxel and advance
            lastVoxel = v;
            lastT     = s.m_t;
            lastAxis  = s.m_axis;
        }

        // Exited the brick without finding empty space — the exit is at
        // the brick boundary. Report the last known t and axis so the
        // outer loop can continue into the next brick.
        ray.m_voxel = lastVoxel;
        ray.m_t     = lastT;
        ray.m_axis  = lastAxis;
        axis      = lastAxis;
        return false;  // outer loop must continue
    }

    bool Occluded(const core::Ray& ray, const BrickMap* map,
                  const uint brickIdx, const float3 brickOrigin, const float entryT)
    {
        InnerDDAState s = setupInnerDda(ray, brickOrigin, entryT, ray.m_axis);
        const uint32_t* voxels = map->m_pBricks[brickIdx].m_aVoxel;

        // Return true immediately on the first solid voxel found
        do {
            if (voxels[s.m_x + s.m_y * BRICKSIZE + s.m_z * BRICKSIZE2]) return true;
        } while (advanceInnerDda(s));

        return false;
    }

    bool Transmittance(const core::Ray& ray, const BrickMap* map,
                       const uint brickIdx, const float3 brickOrigin,
                       const float entryT, const rt::rendering::MaterialManager& materials, float3& transmittance)
    {
        InnerDDAState s = setupInnerDda(ray, brickOrigin, entryT, ray.m_axis);
        const uint32_t* voxels = map->m_pBricks[brickIdx].m_aVoxel;
        const float prevT = entryT;

        do {
            const uint32_t v = voxels[s.m_x + s.m_y * BRICKSIZE + s.m_z * BRICKSIZE2];
            if (v) {
                const rt::rendering::Material& mat = materials.getMaterial(rt::rendering::getMaterialID(v));
                if (mat.m_transparency <= 0.f) return false;  // fully opaque — blocked

                const float segmentLength = s.m_t - prevT;

                // Clamp base color away from 0 to avoid log(0)
                const float3 safeColor = float3(
                    max(mat.m_baseColor.x, 1e-4f),
                    max(mat.m_baseColor.y, 1e-4f),
                    max(mat.m_baseColor.z, 1e-4f));

                // Per-channel absorption coefficient derived from base color
                const float3 sigma = float3(
                    -logf(safeColor.x),
                    -logf(safeColor.y),
                    -logf(safeColor.z));

                // Beer-Lambert: T = exp(-sigma * density * distance)
                const float  density = mat.m_absorptionDensity * mat.m_transparency;
                transmittance *= float3(
                    expf(-sigma.x * density * segmentLength),
                    expf(-sigma.y * density * segmentLength),
                    expf(-sigma.z * density * segmentLength));

                // Early out when transmittance is effectively zero
                if (transmittance.x < EPSILON &&
                    transmittance.y < EPSILON &&
                    transmittance.z < EPSILON)
                    return false;
            }
        } while (advanceInnerDda(s));

        return true;
    }

}  // namespace Tmpl8::BrickTraverser