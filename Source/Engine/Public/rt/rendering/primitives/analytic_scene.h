#pragma once

#include "tmpl8/template.h"
#include "rt/rendering/primitives/sphere.h"

namespace Tmpl8 {
    class Ray;
}

namespace rt::primitives
{
    struct HitInfo;

    // ================================================================
    // AnalyticScene
    //
    // Manages a collection of analytic primitives (currently spheres)
    // that are tested against rays independently of the voxel world.
    //
    // Internally uses tinyBVH for BVH *construction* only. Traversal
    // is done with a custom inlined loop that tests ray-sphere
    // intersections directly, avoiding tinyBVH's callback overhead.
    //
    // The BVH is built over proxy AABBs derived from each sphere's
    // bounding box. At leaf nodes the traversal performs the actual
    // analytic ray-sphere test — no proxy triangles are intersected.
    //
    // Usage:
    //   1. addSphere() to populate the scene
    //   2. buildBvh()  once before rendering (or after sphere changes)
    //   3. intersect()  / isOccluded() per ray during rendering
    // ================================================================
    class AnalyticScene
    {
    public:
        static constexpr uint kMaxSpheres = 4096;

        AnalyticScene();
        ~AnalyticScene();

        // Non-copyable — BVHStorage owns heap memory
        AnalyticScene(const AnalyticScene&) = delete;
        AnalyticScene& operator=(const AnalyticScene&) = delete;

        // --- Sphere management ---
        uint addSphere(const primitives::Sphere& s);
        void removeSphere(uint idx);
        void setSphereCenter(uint idx, const float3& center);
        void clear();

        [[nodiscard]] uint getSphereCount() const { return m_sphereCount; }
        [[nodiscard]] const Sphere& getSphere(const uint idx) const { return m_aSpheres[idx]; }

        // --- BVH construction ---
        // Rebuilds the entire acceleration structure. Call after
        // any change to the sphere list before rendering.
        void buildBvh() const;
        void rebuildSphereBlas() const;

        // --- Ray queries ---
        // Finds the nearest sphere hit closer than hit.m_t.
        // On hit: fills hit with t, normal, matIndex, radius and returns true.
        [[nodiscard]] bool intersect(const core::Ray& ray, HitInfo& hit) const;

        // Returns true if any sphere occludes the ray before ray.m_t.
        // Does not write hit data — used for shadow rays.
        [[nodiscard]] bool isOccluded(const core::Ray& ray) const;

    private:
        Sphere m_aSpheres[kMaxSpheres];
        uint m_sphereCount = 0;

        // tinyBVH data hidden behind an opaque storage struct to keep
        // tiny_bvh.h out of this public header.
        struct BVHStorage;
        BVHStorage* m_pStorage = nullptr;
    };

}
