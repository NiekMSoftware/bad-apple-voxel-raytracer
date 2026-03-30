#pragma once
#include "rt/core/ray.h"
#include "rt/scene/brick_map.h"

namespace rt::scene::internal {

    // --------------------------------------------------------
    // Outer DDA state (GRIDSIZE^3 grid)
    // --------------------------------------------------------
    struct DDAState
    {
        int3   m_step;
        uint   m_x, m_y, m_z;
        float  m_t;
        float3 m_tDelta;
        float3 m_tmax;
        uint   m_axis;
    };

    // --------------------------------------------------------
    // Inner DDA state (BRICKSIZE^3 voxels inside one brick)
    // --------------------------------------------------------
    struct InnerDDAState
    {
        int    m_x, m_y, m_z;
        float  m_t;
        int3   m_step;
        float3 m_tmax;
        float3 m_tdelta;
        uint   m_axis;
    };

    inline InnerDDAState setupInnerDda(const core::Ray& ray, const float3& brickOrigin, const float entryT, const uint outerAxis)
    {
        const float3 entryPoint = ray.m_o + (entryT + 0.00005f) * ray.m_d;
        const float3 posInBrick = static_cast<float>(BRICKSIZE) *
                                  (entryPoint - brickOrigin) * static_cast<float>(GRIDSIZE);
        const int3   P          = clamp(make_int3(posInBrick), 0, BRICKSIZE - 1);
        const int3   step       = make_int3(1.0f - ray.m_dsign * 2.0f);

        const float3 gridPlanes = (ceilf(posInBrick) - ray.m_dsign) * VOXELSIZE;
        const float3 tmax       = (gridPlanes + brickOrigin - ray.m_o) * ray.m_rD;
        const float3 tdelta     = VOXELSIZE * float3(step) * ray.m_rD;

        return { P.x, P.y, P.z, entryT, step, tmax, tdelta, outerAxis };
    }

    inline bool advanceInnerDda(InnerDDAState& s)
    {
        if (s.m_tmax.x < s.m_tmax.y) {
            if (s.m_tmax.x < s.m_tmax.z) { s.m_t = s.m_tmax.x; s.m_x += s.m_step.x; s.m_axis = 0; if (s.m_x < 0 || s.m_x >= BRICKSIZE) return false; s.m_tmax.x += s.m_tdelta.x; }
            else                     { s.m_t = s.m_tmax.z; s.m_z += s.m_step.z; s.m_axis = 2; if (s.m_z < 0 || s.m_z >= BRICKSIZE) return false; s.m_tmax.z += s.m_tdelta.z; }
        } else {
            if (s.m_tmax.y < s.m_tmax.z) { s.m_t = s.m_tmax.y; s.m_y += s.m_step.y; s.m_axis = 1; if (s.m_y < 0 || s.m_y >= BRICKSIZE) return false; s.m_tmax.y += s.m_tdelta.y; }
            else                     { s.m_t = s.m_tmax.z; s.m_z += s.m_step.z; s.m_axis = 2; if (s.m_z < 0 || s.m_z >= BRICKSIZE) return false; s.m_tmax.z += s.m_tdelta.z; }
        }
        return true;
    }

    // --------------------------------------------------------
    // Super DDA state (SUPERGRIDSIZE^3 = 8^3 coarse grid)
    //
    // Each super-cell covers 8x8x8 outer grid cells.
    // The super DDA steps through this coarse grid first;
    // only when a super-cell is occupied does the caller
    // descend into the fine 64^3 DDA within that region.
    //
    // Reference: "A Rundown on Brickmaps" (uygarb.dev) —
    //   the same two-level skip principle applied as a
    //   third level atop the existing brick/voxel hierarchy.
    // --------------------------------------------------------
    struct SuperDDAState
    {
        int3   m_step;
        uint   m_x, m_y, m_z;
        float  m_t;
        float3 m_tDelta;
        float3 m_tmax;
        uint   m_axis;
    };

    // Setup the super-grid DDA. Returns false if the ray
    // misses the unit cube entirely (same logic as Setup3DDDA
    // but at SUPERGRIDSIZE resolution).
    inline bool setupSuperDda(const core::Ray& ray, SuperDDAState& state)
    {
        state.m_t = 0.f;

        const bool startedInGrid = (ray.m_o.x >= 0.f && ray.m_o.y >= 0.f && ray.m_o.z >= 0.f &&
                                    ray.m_o.x <= 1.f && ray.m_o.y <= 1.f && ray.m_o.z <= 1.f);
        if (!startedInGrid)
        {
            // Intersect the unit cube [0,1]^3
            const float tx1 = -ray.m_o.x * ray.m_rD.x, tx2 = (1.f - ray.m_o.x) * ray.m_rD.x;
            float tmin = min(tx1, tx2), tmax = max(tx1, tx2);
            const float ty1 = -ray.m_o.y * ray.m_rD.y, ty2 = (1.f - ray.m_o.y) * ray.m_rD.y;
            tmin = max(tmin, min(ty1, ty2)); tmax = min(tmax, max(ty1, ty2));
            const float tz1 = -ray.m_o.z * ray.m_rD.z, tz2 = (1.f - ray.m_o.z) * ray.m_rD.z;
            tmin = max(tmin, min(tz1, tz2)); tmax = min(tmax, max(tz1, tz2));
            if (tmax < tmin || tmin <= 0.f) return false;
            state.m_t = tmin;
        }

        state.m_step = make_int3(1.0f - ray.m_dsign * 2.0f);

        const float3 posInGrid  = static_cast<float>(SUPERGRIDSIZE) * (ray.m_o + (state.m_t + 0.00005f) * ray.m_d);
        const float3 gridPlanes = (ceilf(posInGrid) - ray.m_dsign) * SUPERCELLSIZE;
        const int3   P          = clamp(make_int3(posInGrid), 0, SUPERGRIDSIZE - 1);

        state.m_x      = P.x; state.m_y = P.y; state.m_z = P.z;
        state.m_tDelta = SUPERCELLSIZE * float3(state.m_step) * ray.m_rD;
        state.m_tmax   = (gridPlanes - ray.m_o) * ray.m_rD;
        state.m_axis   = 0;

        return true;
    }

    // Advance the super DDA by one super-cell.
    // Returns false when the ray exits the SUPERGRIDSIZE^3 grid.
    inline bool advanceSuperDda(SuperDDAState& state, uint& axis)
    {
        if (state.m_tmax.x < state.m_tmax.y) {
            if (state.m_tmax.x < state.m_tmax.z) { state.m_t = state.m_tmax.x; state.m_x += state.m_step.x; axis = 0; if (state.m_x >= SUPERGRIDSIZE) return false; state.m_tmax.x += state.m_tDelta.x; }
            else                                  { state.m_t = state.m_tmax.z; state.m_z += state.m_step.z; axis = 2; if (state.m_z >= SUPERGRIDSIZE) return false; state.m_tmax.z += state.m_tDelta.z; }
        } else {
            if (state.m_tmax.y < state.m_tmax.z) { state.m_t = state.m_tmax.y; state.m_y += state.m_step.y; axis = 1; if (state.m_y >= SUPERGRIDSIZE) return false; state.m_tmax.y += state.m_tDelta.y; }
            else                                  { state.m_t = state.m_tmax.z; state.m_z += state.m_step.z; axis = 2; if (state.m_z >= SUPERGRIDSIZE) return false; state.m_tmax.z += state.m_tDelta.z; }
        }
        return true;
    }

    // Outer DDA - steps one outer grid cell.
    // Returns false when the ray exits the GRIDSIZE^3 grid.
    inline bool advanceDda(DDAState& state, uint& axis)
    {
        if (state.m_tmax.x < state.m_tmax.y) {
            if (state.m_tmax.x < state.m_tmax.z) { state.m_t = state.m_tmax.x; state.m_x += state.m_step.x; axis = 0; if (state.m_x >= GRIDSIZE) return false; state.m_tmax.x += state.m_tDelta.x; }
            else                             { state.m_t = state.m_tmax.z; state.m_z += state.m_step.z; axis = 2; if (state.m_z >= GRIDSIZE) return false; state.m_tmax.z += state.m_tDelta.z; }
        } else {
            if (state.m_tmax.y < state.m_tmax.z) { state.m_t = state.m_tmax.y; state.m_y += state.m_step.y; axis = 1; if (state.m_y >= GRIDSIZE) return false; state.m_tmax.y += state.m_tDelta.y; }
            else                             { state.m_t = state.m_tmax.z; state.m_z += state.m_step.z; axis = 2; if (state.m_z >= GRIDSIZE) return false; state.m_tmax.z += state.m_tDelta.z; }
        }
        return true;
    }

}  // namespace Tmpl8