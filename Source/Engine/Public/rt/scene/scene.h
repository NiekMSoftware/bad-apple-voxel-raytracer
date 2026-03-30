#pragma once
#include "rt/core/ray.h"
#include "rt/scene/brick_pool.h"
#include "rt/rendering/primitives/voxel_instance.h"
#include "rt/rendering/mesh.h"
#include <vector>
#include <memory>
#include "rt/rendering/primitives/analytic_scene.h"

namespace rt::rendering {
    class MaterialManager;
}

namespace rt::scene {

    // ========================================================================
    // Sentinel stored in ray.m_voxel for TLAS instance voxel hits.
    //
    // Bit 29 only.  Safe because material IDs 0–255 occupy bits 31–24
    // (values 0x00–0xFF), none of which set bit 29.  The lower 24 bits
    // carry the voxel RGB so that ray.getAlbedo() keeps working.
    //
    // Other sentinels for reference:
    //   0x80000000u | prim  - mesh hit   (bit 31)
    //   0x40000000u         - analytic hit (bit 30)
    //   0x20000000u         - instance voxel hit (bit 29)  <- this one
    // ========================================================================
    static constexpr uint k_instanceVoxelSentinel = 0x20000000u;

    // ============================================================
    // Scene
    // ============================================================
    class Scene
    {
    public:
        Scene()  = default;
        ~Scene() = default;

        // Finds the nearest intersection along the ray (voxel, instance, or mesh).
        void findNearest(core::Ray& ray, primitives::HitInfo& hitInfo) const;

        // Returns true if any geometry occludes the ray before ray.t.
        bool isOccluded(core::Ray& ray) const;
        bool isOccluded(core::Ray& ray, const rt::rendering::MaterialManager& materials) const;

        // Returns the accumulated transmittance along the ray.
        float3 transmittance(core::Ray& ray, const rt::rendering::MaterialManager& materials) const;

        // Write a voxel into the main world BrickPool.
        void set(const uint x, const uint y, const uint z, const uint v) { m_pool.set(x, y, z, v); }

        // Builds the BVH for a mesh and appends it to the mesh list.
        void addMesh(rt::rendering::Mesh&& m);

        // ----------------------------------------------------------------
        // TLAS instance management
        //
        // Adds a new VoxelInstance to the TLAS with the given world-space
        // pivot and returns a non-owning pointer to it.  The Scene owns
        // the instance for its entire lifetime.
        //
        // The instance's BLAS (m_blas BrickPool) is empty after this call.
        // Fill it with voxel geometry via instance->m_blas.set(...) and
        // call instance->setRotationY(0.f) to initialise the transform.
        //
        // Per-frame rotation update: call instance->setRotationY(angle).
        // No voxel rewrites are needed - only the mat4 changes.
        // ----------------------------------------------------------------
        VoxelInstance* addInstance(float3 centre);

        BrickPool                        m_pool;
        std::vector<rt::rendering::Mesh> m_vMeshes;
        rt::primitives::AnalyticScene    m_analyticScene;

    private:
        // unique_ptr because VoxelInstance is non-copyable (owns a BrickPool).
        std::vector<std::unique_ptr<VoxelInstance>> m_vInstances;
    };

}  // namespace rt::scene