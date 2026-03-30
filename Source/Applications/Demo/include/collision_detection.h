#pragma once
#include "tmpl8/template.h"
#include "rt/scene/scene.h"
#include "rt/core/ray.h"
#include "rt/rendering/primitives/hit_info.h"

namespace demo
{
    inline bool SphereCast(
        const rt::scene::Scene& scene,
        const float3& pos, const float radius,
        const float3& velocity,
        float3& outNormal, float& outDepth)
    {
        const float3 directions[] = {
            {0, -1, 0}, {0, 1, 0},
            {1, 0, 0}, {-1, 0, 0},
            {0, 0, 1}, {0, 0, -1},
            normalize(float3(1, -1, 0)),
            normalize(float3(-1, -1, 0))
        };

        bool hit = false;
        outNormal = float3(0.f);
        outDepth = 0.f;

        for (auto direction : directions)
        {
            rt::core::Ray ray(pos, direction, radius);
            rt::primitives::HitInfo hitInfo{};
            scene.findNearest(ray, hitInfo);

            // Skip self-intersection: the physics sphere is in the analytic
            // scene, so rays from its centre hit its own surface at t ≈ radius.
            // Any real collision with the environment will be at t < radius
            // AND not near t ≈ radius (the sphere's own shell).
            if (ray.m_t >= radius) continue;
            if (ray.m_voxel == 0x40000000u && hitInfo.m_matIndex == 12) continue;  // only skip gold spheres (dirty code but hey, it works)
            outNormal += ray.getNormal();
            outDepth = max(outDepth, radius - ray.m_t);
            hit = true;
        }

        if (length(velocity) > 0.001f)
        {
            rt::core::Ray ray(pos, normalize(velocity), radius);
            rt::primitives::HitInfo hitInfo{};
            scene.findNearest(ray, hitInfo);

            if (ray.m_t <= radius && ray.m_voxel != 0x40000000u)
            {
                outNormal += ray.getNormal();
                outDepth = max(outDepth, radius - ray.m_t);
                hit = true;
            }
        }

        if (hit) outNormal = normalize(outNormal);
        return hit;
    }
}
