#pragma once

// ============================================================================
// VoxelInstance — one TLAS entry for a transformed voxel object.
//
// The bounding sphere guards every shadow / transmittance test:
//   - radius = sqrt(3) * halfSizeVoxels / WORLDSIZE
//     This encloses the full cube diagonal in all axes for any rotation.
//     Reference: the circumsphere of a cube of half-side h has radius h*sqrt(3)
//     https://en.wikipedia.org/wiki/Circumscribed_sphere
//
// Ray transform (pure rotation around a world-space pivot):
//   local_o = R⁻¹ * (world_o − pivot) + pivot
//   local_d = R⁻¹ * world_d
//   For orthogonal R: R⁻¹ = Rᵀ
//   Reference: Ray Tracing Gems ch. 19 — "Transforming the Ray Instead of the Objects"
//   https://www.realtimerendering.com/raytracinggems/
// ============================================================================

#include "rt/scene/brick_pool.h"
#include "rt/core/ray.h"
#include "tmpl8/tmpl8math.h"

namespace rt::scene {

    struct VoxelInstance
    {
        BrickPool m_blas;

        float3 m_centre{ 0.5f, 0.5f, 0.5f };   // world-space pivot
        float  m_boundingRadius = 0.0f;          // circumsphere radius in world units

        mat4 m_rotation{};      // object -> world
        mat4 m_invRotation{};   // world -> object (= transpose for pure rotation)

        VoxelInstance()  = default;
        ~VoxelInstance() = default;

        VoxelInstance(const VoxelInstance&)            = delete;
        VoxelInstance& operator=(const VoxelInstance&) = delete;

        // ====================================================================
        // setBoundingRadius
        //
        // Call once after the BLAS geometry is known.
        // halfSizeVoxels = the half-side length of the cube in voxel counts.
        // ====================================================================
        void setBoundingRadius(const int halfSizeVoxels)
        {
            // sqrt(3) * halfSize is the exact circumsphere radius of the cube.
            // Add 2 voxels of margin to prevent floating-point clipping of corners.
            m_boundingRadius = (sqrtf(3.0f) * static_cast<float>(halfSizeVoxels) + 2.0f)
                             / static_cast<float>(WORLDSIZE);
        }

        // ====================================================================
        // setRotationXYZ
        //
        // Builds the combined rotation matrix R = Rx(ax) * Ry(ay) * Rz(az)
        // and its inverse (transpose, since R is orthogonal).
        //
        // Combined matrix entries (row-major, cell[row*4+col]):
        // Reference: https://en.wikipedia.org/wiki/Rotation_matrix#General_rotations
        // ====================================================================
        void setRotationXYZ(const float ax, const float ay, const float az)
        {
            const float cx = cosf(ax), sx = sinf(ax);
            const float cy = cosf(ay), sy = sinf(ay);
            const float cz = cosf(az), sz = sinf(az);

            // Forward: R = Rx * Ry * Rz
            m_rotation.cell[0]  =  cy*cz;
            m_rotation.cell[1]  = -cy*sz;
            m_rotation.cell[2]  =  sy;
            m_rotation.cell[3]  =  0.f;
            m_rotation.cell[4]  =  sx*sy*cz + cx*sz;
            m_rotation.cell[5]  = -sx*sy*sz + cx*cz;
            m_rotation.cell[6]  = -sx*cy;
            m_rotation.cell[7]  =  0.f;
            m_rotation.cell[8]  = -cx*sy*cz + sx*sz;
            m_rotation.cell[9]  =  cx*sy*sz + sx*cz;
            m_rotation.cell[10] =  cx*cy;
            m_rotation.cell[11] =  0.f;
            m_rotation.cell[12] =  0.f;
            m_rotation.cell[13] =  0.f;
            m_rotation.cell[14] =  0.f;
            m_rotation.cell[15] =  1.f;

            // Inverse = transpose of forward (orthogonal matrix)
            m_invRotation.cell[0]  =  cy*cz;
            m_invRotation.cell[1]  =  sx*sy*cz + cx*sz;
            m_invRotation.cell[2]  = -cx*sy*cz + sx*sz;
            m_invRotation.cell[3]  =  0.f;
            m_invRotation.cell[4]  = -cy*sz;
            m_invRotation.cell[5]  = -sx*sy*sz + cx*cz;
            m_invRotation.cell[6]  =  cx*sy*sz + sx*cz;
            m_invRotation.cell[7]  =  0.f;
            m_invRotation.cell[8]  =  sy;
            m_invRotation.cell[9]  = -sx*cy;
            m_invRotation.cell[10] =  cx*cy;
            m_invRotation.cell[11] =  0.f;
            m_invRotation.cell[12] =  0.f;
            m_invRotation.cell[13] =  0.f;
            m_invRotation.cell[14] =  0.f;
            m_invRotation.cell[15] =  1.f;
        }

        // Keep single-axis version as a convenience wrapper
        void setRotationY(const float angleRad) { setRotationXYZ(0.f, angleRad, 0.f); }

        // ====================================================================
        // rayIntersectsSphere
        //
        // Returns true if the ray passes within m_boundingRadius of m_centre
        // and the closest approach is within [0, ray.m_t].
        //
        // Standard geometric sphere-ray intersection:
        //   project centre onto ray, measure perp distance squared.
        // Reference: Shirley & Morley, "Realistic Ray Tracing" 2nd ed., Ch. 10.2
        // ====================================================================
        [[nodiscard]] bool rayIntersectsSphere(const core::Ray& ray) const
        {
            const float3 oc = m_centre - ray.m_o;
            const float  t  = dot(oc, ray.m_d);           // closest approach along ray
            const float3 q  = oc - t * ray.m_d;
            const float  d2 = dot(q, q);                  // perp distance squared
            if (d2 > m_boundingRadius * m_boundingRadius) return false;
            const float half = sqrtf(m_boundingRadius * m_boundingRadius - d2);
            const float tMin = t - half;
            const float tMax = t + half;
            return tMax >= 0.0f && tMin <= ray.m_t;
        }

        // ====================================================================
        // transformRayToLocal
        // ====================================================================
        [[nodiscard]] core::Ray transformRayToLocal(const core::Ray& worldRay) const
        {
            const float3 rel      = worldRay.m_o - m_centre;
            const float3 localRel = TransformVector(rel,          m_rotation);
            const float3 localDir = TransformVector(worldRay.m_d, m_rotation);
            const float3 localO   = localRel + m_centre;
            return core::Ray(localO, localDir, worldRay.m_t);
        }

        // ====================================================================
        // transformNormalToWorld
        // ====================================================================
        [[nodiscard]] float3 transformNormalToWorld(const float3& localNormal) const
        {
            return normalize(TransformVector(localNormal, m_invRotation));
        }
    };

}  // namespace rt::scene