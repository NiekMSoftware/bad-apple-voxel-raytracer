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