#include "rt/scene/scene.h"

#include "brick_traverser.h"
#include "dda.h"
#include "tmpl8/template.h"
#include "rt/rendering/material_manager.h"
#include "rt/rendering/primitives/hit_info.h"

namespace rt::scene {

    VoxelInstance* Scene::addInstance(const float3 centre)
    {
        auto inst      = std::make_unique<VoxelInstance>();
        inst->m_centre = centre;
        inst->setRotationY(0.0f);
        VoxelInstance* ptr = inst.get();
        m_vInstances.push_back(std::move(inst));
        return ptr;
    }

    void Scene::addMesh(rt::rendering::Mesh&& m)
    {
        m.build();
        m_vMeshes.push_back(std::move(m));
    }

    // =========================================================================
    // Helper: run the world DDA for findNearest (no goto version)
    //
    // Separated so the inside/outside paths are plain if/else with no jumps.
    // Returns true if the DDA found a hit and wrote ray.m_voxel / ray.m_t.
    //
    // Super-grid acceleration is NOT used here because findNearest
    // requires correct ray.m_axis for normal reconstruction (getNormal).
    // Setup3DDDAAt does not correctly propagate the entry axis when
    // the fine DDA starts mid-ray, corrupting normals. The isOccluded
    // paths don't have this problem — they only return true/false and
    // never read ray.m_axis.
    // =========================================================================
    static bool worldDdaNearest(core::Ray& ray, const BrickMap* map)
    {
        internal::DDAState s{};
        if (!internal::Setup3DDDA(map, ray, s)) return false;

        uint   axis = s.m_axis;
        uint   brickIdx;
        float3 brickOrigin{};

        if (ray.m_bInside)
        {
            // Ray started inside solid — walk until we exit the solid.
            uint lastVoxel = 0;
            while (true)
            {
                if (internal::GetBrickInfo(map, s.m_x, s.m_y, s.m_z, brickIdx, brickOrigin))
                {
                    if (internal::Nearest(ray, map, brickIdx, brickOrigin, s.m_t, axis, true))
                        return true;
                    lastVoxel = ray.m_voxel;
                }
                else
                {
                    // Stepped into empty outer cell while inside — surface is here
                    ray.m_voxel = lastVoxel;
                    ray.m_t     = s.m_t;
                    ray.m_axis  = s.m_axis;
                    return true;
                }
                if (!internal::advanceDda(s, axis)) break;
            }
            return false;
        }

        // Normal forward traversal
        while (true)
        {
            if (internal::GetBrickInfo(map, s.m_x, s.m_y, s.m_z, brickIdx, brickOrigin))
                if (internal::Nearest(ray, map, brickIdx, brickOrigin, s.m_t, axis))
                    return true;
            if (!internal::advanceDda(s, axis)) break;
        }
        return false;
    }

    // =========================================================================
    // findNearest
    // =========================================================================
    void Scene::findNearest(core::Ray& ray) const
    {
        ray.m_o += EPSILON * ray.m_d;

        const float maxT = ray.m_t;

        // 1. World BrickPool
        worldDdaNearest(ray, m_pool.getMap());

        // 2. TLAS instances — no sphere guard needed here, Setup3DDDA handles misses
        for (const auto& inst : m_vInstances)
        {
            core::Ray testRay(ray);
            testRay.m_t = maxT;
            if (!inst->rayIntersectsSphere(testRay)) continue;

            core::Ray lr = inst->transformRayToLocal(ray);

            internal::DDAState s{};
            const BrickMap* map = inst->m_blas.getMap();
            if (!internal::Setup3DDDA(map, lr, s)) continue;

            uint   axis = s.m_axis;
            uint   brickIdx;
            float3 brickOrigin{};

            while (true)
            {
                if (internal::GetBrickInfo(map, s.m_x, s.m_y, s.m_z, brickIdx, brickOrigin))
                    if (internal::Nearest(lr, map, brickIdx, brickOrigin, s.m_t, axis))
                        break;
                if (!internal::advanceDda(s, axis)) break;
            }

            if (lr.m_voxel == 0)   continue;
            if (lr.m_t >= ray.m_t) continue;

            ray.m_t            = lr.m_t;
            ray.m_primNormal   = inst->transformNormalToWorld(lr.getNormal());
            ray.m_primMatIndex = lr.m_voxel >> 24;
            ray.m_voxel        = k_instanceVoxelSentinel | (lr.m_voxel & 0x00FFFFFFu);
        }

        // 3. Mesh BVH
        for (const auto& [m_vVertices, m_bvh] : m_vMeshes)
        {
            tinybvh::Ray tbvhRay(
                tinybvh::bvhvec3(ray.m_o.x, ray.m_o.y, ray.m_o.z),
                tinybvh::bvhvec3(ray.m_d.x, ray.m_d.y, ray.m_d.z),
                ray.m_t);
            m_bvh.Intersect(tbvhRay);
            if (tbvhRay.hit.t < ray.m_t)
            {
                ray.m_t     = tbvhRay.hit.t;
                ray.m_voxel = 0x80000000u | tbvhRay.hit.prim;
            }
        }

        // 4. Analytic scene
        rt::primitives::HitInfo hit;
        hit.m_t = ray.m_t;
        if (m_analyticScene.intersect(ray, hit))
        {
            ray.m_t            = hit.m_t;
            ray.m_voxel        = 0x40000000u;
            ray.m_primNormal   = hit.m_normal;
            ray.m_primMatIndex = hit.m_matIndex;
            ray.m_primRadius   = hit.m_radius;
        }
    }

    // =========================================================================
    // isOccluded (no material check)
    //
    // Uses a two-tier fast path before falling back to the full fine DDA:
    //
    //   1. Y-slice check: if the ray travels purely upward (or downward)
    //      and all Y-slices in that direction are empty, the world DDA
    //      is skipped entirely.  Cost: one 64-bit mask test.
    //
    //   2. Super-grid DDA (8^3): steps through the coarse grid and only
    //      enters the fine 64^3 DDA within occupied super-cells.  For
    //      shadow rays traversing mostly empty space this reduces the
    //      step count from ~64 to ~8.
    //
    // Reference (super-grid skip):
    //   "A Rundown on Brickmaps" — uygarb.dev
    //   VoxelRT MultiDDA — github.com/dubiousconst282/VoxelRT
    // =========================================================================
    bool Scene::isOccluded(core::Ray& ray) const
    {
        ray.m_o += EPSILON * ray.m_d;
        ray.m_t -= EPSILON * 2.0f;

        const BrickMap* map = m_pool.getMap();

        // World DDA with super-grid acceleration
        {
            // --- Tier 1: Y-slice early-out ---
            // If the ray travels upward/downward and there is no geometry
            // in any Y-slice along its path, the entire world DDA is skipped.
            bool skipWorldDda = false;
            {
                // Determine the outer-grid Y of the ray origin (clamped to grid)
                const float entryY = clamp(ray.m_o.y, 0.0f, 1.0f - VOXELSIZE);
                const uint  originY = static_cast<uint>(entryY * GRIDSIZE);

                if (ray.m_d.y > 0.0f)
                    skipWorldDda = map->allYSlicesAboveEmpty(originY);
                else if (ray.m_d.y < 0.0f)
                    skipWorldDda = map->allYSlicesBelowEmpty(originY);
                // For perfectly horizontal rays (m_d.y == 0) we cannot skip.
            }

            if (!skipWorldDda)
            {
                // --- Tier 2: super-grid DDA (8^3) into fine-grid DDA (64^3) ---
                internal::SuperDDAState ss{};
                if (internal::setupSuperDda(ray, ss))
                {
                    uint superAxis = ss.m_axis;
                    while (ss.m_t < ray.m_t)
                    {
                        const uint superIdx = ss.m_x + ss.m_y * SUPERGRIDSIZE + ss.m_z * SUPERGRIDSIZE2;
                        if (map->isSuperOccupied(superIdx))
                        {
                            internal::DDAState s{};
                            internal::Setup3DDDAAt(map, ray, s, ss.m_t);

                            const uint fineMinX = ss.m_x * 8, fineMaxX = fineMinX + 8;
                            const uint fineMinY = ss.m_y * 8, fineMaxY = fineMinY + 8;
                            const uint fineMinZ = ss.m_z * 8, fineMaxZ = fineMinZ + 8;

                            uint axis = s.m_axis;
                            uint brickIdx; float3 brickOrigin{};

                            while (s.m_t < ray.m_t)
                            {
                                if (s.m_x < fineMinX || s.m_x >= fineMaxX ||
                                    s.m_y < fineMinY || s.m_y >= fineMaxY ||
                                    s.m_z < fineMinZ || s.m_z >= fineMaxZ)
                                    break;

                                if (internal::GetBrickInfo(map, s.m_x, s.m_y, s.m_z, brickIdx, brickOrigin))
                                    if (internal::Occluded(ray, map, brickIdx, brickOrigin, s.m_t))
                                        return true;
                                if (!internal::advanceDda(s, axis)) break;
                            }
                        }
                        if (!internal::advanceSuperDda(ss, superAxis)) break;
                    }
                }
            }
        }

        // Instances — bounding sphere guard
        for (const auto& inst : m_vInstances)
        {
            if (!inst->rayIntersectsSphere(ray)) continue;

            core::Ray lr = inst->transformRayToLocal(ray);
            internal::DDAState s{};
            const BrickMap* map2 = inst->m_blas.getMap();
            if (!internal::Setup3DDDA(map2, lr, s)) continue;
            if (lr.m_bInside) continue;   // ray started inside this instance — skip self-shadow

            uint axis = s.m_axis, brickIdx; float3 brickOrigin{};
            while (s.m_t < lr.m_t)
            {
                if (internal::GetBrickInfo(map2, s.m_x, s.m_y, s.m_z, brickIdx, brickOrigin))
                    if (internal::Occluded(lr, map2, brickIdx, brickOrigin, s.m_t))
                        return true;
                if (!internal::advanceDda(s, axis)) break;
            }
        }

        // Meshes
        for (const auto& mesh : m_vMeshes)
        {
            tinybvh::Ray tbvhRay(
                tinybvh::bvhvec3(ray.m_o.x, ray.m_o.y, ray.m_o.z),
                tinybvh::bvhvec3(ray.m_d.x, ray.m_d.y, ray.m_d.z),
                ray.m_t);
            if (mesh.m_bvh.IsOccluded(tbvhRay)) return true;
        }

        if (m_analyticScene.isOccluded(ray)) return true;
        return false;
    }

    // =========================================================================
    // isOccluded (with material transparency)
    //
    // Same Y-slice + super-grid acceleration as the simple overload.
    // =========================================================================
    bool Scene::isOccluded(core::Ray& ray, const rt::rendering::MaterialManager& materials) const
    {
        ray.m_o += EPSILON * ray.m_d;
        ray.m_t -= EPSILON * 2.0f;

        const BrickMap* map = m_pool.getMap();

        // World DDA with super-grid acceleration
        {
            // --- Tier 1: Y-slice early-out ---
            bool skipWorldDda = false;
            {
                const float entryY = clamp(ray.m_o.y, 0.0f, 1.0f - VOXELSIZE);
                const uint  originY = static_cast<uint>(entryY * GRIDSIZE);

                if (ray.m_d.y > 0.0f)
                    skipWorldDda = map->allYSlicesAboveEmpty(originY);
                else if (ray.m_d.y < 0.0f)
                    skipWorldDda = map->allYSlicesBelowEmpty(originY);
            }

            if (!skipWorldDda)
            {
                // --- Tier 2: super-grid DDA ---
                internal::SuperDDAState ss{};
                if (internal::setupSuperDda(ray, ss))
                {
                    uint superAxis = ss.m_axis;
                    while (ss.m_t < ray.m_t)
                    {
                        const uint superIdx = ss.m_x + ss.m_y * SUPERGRIDSIZE + ss.m_z * SUPERGRIDSIZE2;
                        if (map->isSuperOccupied(superIdx))
                        {
                            internal::DDAState s{};
                            internal::Setup3DDDAAt(map, ray, s, ss.m_t);

                            const uint fineMinX = ss.m_x * 8, fineMaxX = fineMinX + 8;
                            const uint fineMinY = ss.m_y * 8, fineMaxY = fineMinY + 8;
                            const uint fineMinZ = ss.m_z * 8, fineMaxZ = fineMinZ + 8;

                            uint axis = s.m_axis;
                            uint brickIdx; float3 brickOrigin{};

                            while (s.m_t < ray.m_t)
                            {
                                if (s.m_x < fineMinX || s.m_x >= fineMaxX ||
                                    s.m_y < fineMinY || s.m_y >= fineMaxY ||
                                    s.m_z < fineMinZ || s.m_z >= fineMaxZ)
                                    break;

                                if (internal::GetBrickInfo(map, s.m_x, s.m_y, s.m_z, brickIdx, brickOrigin))
                                    if (internal::Occluded(ray, map, brickIdx, brickOrigin, s.m_t))
                                        return true;
                                if (!internal::advanceDda(s, axis)) break;
                            }
                        }
                        if (!internal::advanceSuperDda(ss, superAxis)) break;
                    }
                }
            }
        }

        // Instances — bounding sphere guard
        for (const auto& inst : m_vInstances)
        {
            if (!inst->rayIntersectsSphere(ray)) continue;

            core::Ray lr = inst->transformRayToLocal(ray);
            internal::DDAState s{};
            const BrickMap* map2 = inst->m_blas.getMap();
            if (!internal::Setup3DDDA(map2, lr, s)) continue;
            if (lr.m_bInside) continue;   // ray started inside this instance — skip self-shadow

            uint axis = s.m_axis, brickIdx; float3 brickOrigin{};
            while (s.m_t < lr.m_t)
            {
                if (internal::GetBrickInfo(map2, s.m_x, s.m_y, s.m_z, brickIdx, brickOrigin))
                    if (internal::Occluded(lr, map2, brickIdx, brickOrigin, s.m_t))
                        return true;
                if (!internal::advanceDda(s, axis)) break;
            }
        }

        // Meshes
        for (const auto& mesh : m_vMeshes)
        {
            tinybvh::Ray tbvhRay(
                tinybvh::bvhvec3(ray.m_o.x, ray.m_o.y, ray.m_o.z),
                tinybvh::bvhvec3(ray.m_d.x, ray.m_d.y, ray.m_d.z),
                ray.m_t);
            if (mesh.m_bvh.IsOccluded(tbvhRay)) return true;
        }

        rt::primitives::HitInfo hit;
        hit.m_t = ray.m_t;
        if (m_analyticScene.intersect(ray, hit))
        {
            const rt::rendering::Material& mat = materials.getMaterial(hit.m_matIndex);
            if (mat.m_transparency <= 0.0f) return true;
        }

        return false;
    }

    // =========================================================================
    // transmittance
    // =========================================================================
    float3 Scene::transmittance(core::Ray& ray, const rt::rendering::MaterialManager& materials) const
    {
        ray.m_o += EPSILON * ray.m_d;
        ray.m_t -= EPSILON * 2.0f;

        float3 result{ 1.0f };

        // World DDA
        {
            internal::DDAState s{};
            const BrickMap* map = m_pool.getMap();
            if (internal::Setup3DDDA(map, ray, s))
            {
                uint axis = s.m_axis, brickIdx; float3 brickOrigin{};
                while (s.m_t < ray.m_t)
                {
                    if (internal::GetBrickInfo(map, s.m_x, s.m_y, s.m_z, brickIdx, brickOrigin))
                        if (!internal::Transmittance(ray, map, brickIdx, brickOrigin,
                                                     s.m_t, materials, result))
                            return { 0.0f };
                    if (!internal::advanceDda(s, axis)) break;
                }
            }
        }

        // Instances — bounding sphere guard, treated as opaque blockers
        for (const auto& inst : m_vInstances)
        {
            if (!inst->rayIntersectsSphere(ray)) continue;

            core::Ray lr = inst->transformRayToLocal(ray);
            internal::DDAState s{};
            const BrickMap* map = inst->m_blas.getMap();
            if (!internal::Setup3DDDA(map, lr, s)) continue;
            if (lr.m_bInside) continue;   // ray started inside this instance — skip self-shadow

            uint axis = s.m_axis, brickIdx; float3 brickOrigin{};
            while (s.m_t < lr.m_t)
            {
                if (internal::GetBrickInfo(map, s.m_x, s.m_y, s.m_z, brickIdx, brickOrigin))
                    if (internal::Occluded(lr, map, brickIdx, brickOrigin, s.m_t))
                        return { 0.0f };
                if (!internal::advanceDda(s, axis)) break;
            }
        }

        // Analytic sphere transmittance
        rt::primitives::HitInfo hit;
        hit.m_t = ray.m_t;
        if (m_analyticScene.intersect(ray, hit))
        {
            const rt::rendering::Material& mat = materials.getMaterial(hit.m_matIndex);
            if (mat.m_transparency <= 0.0f) return { 0.0f };

            const float r0 = ((1.0f - mat.m_indexOfRefraction) / (1.0f + mat.m_indexOfRefraction));
            const float r0sq = r0 * r0;
            const float fresnelT = (1.0f - r0sq) * (1.0f - r0sq);
            const float3 safeColor = float3(
                max(mat.m_baseColor.x, 1e-4f),
                max(mat.m_baseColor.y, 1e-4f),
                max(mat.m_baseColor.z, 1e-4f));
            const float3 sigma = float3(
                -logf(safeColor.x), -logf(safeColor.y), -logf(safeColor.z));
            const float density     = mat.m_absorptionDensity * mat.m_transparency;
            const float chordLength = hit.m_t / (2.0f * hit.m_radius);
            result *= fresnelT * float3(
                expf(-sigma.x * density * chordLength),
                expf(-sigma.y * density * chordLength),
                expf(-sigma.z * density * chordLength));
        }

        return result;
    }

}  // namespace rt::scene