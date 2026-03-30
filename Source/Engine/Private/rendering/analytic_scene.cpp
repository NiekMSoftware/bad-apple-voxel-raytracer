#pragma warning(push)
#pragma warning(disable: 4996)
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"
#pragma warning(pop)

#include "rt/rendering/primitives/analytic_scene.h"
#include "rt/rendering/primitives/hit_info.h"
#include "rt/core/ray.h"

namespace rt::primitives
{
    static const Sphere* s_kpSpheres = nullptr;

    // Callback 1: Compute AABB for primitive 'primIdx'
    static void sphereBounds(const uint32_t primIdx, tinybvh::bvhvec3& aabbMin, tinybvh::bvhvec3& aabbMax)
    {
        const auto& s = s_kpSpheres[primIdx];
        aabbMin = tinybvh::bvhvec3(
            s.m_center.x - s.m_radius,
            s.m_center.y - s.m_radius,
            s.m_center.z - s.m_radius);
        aabbMax = tinybvh::bvhvec3(
            s.m_center.x + s.m_radius,
            s.m_center.y + s.m_radius,
            s.m_center.z + s.m_radius);
    }

    // Callback 2: Intersect ray with sphere 'primIdx', update ray.hit if closer
    static bool sphereIntersect(tinybvh::Ray& ray, const uint32_t primIdx)
    {
        const auto& s = s_kpSpheres[primIdx];
        const float cx = s.m_center.x - ray.O.x;
        const float cy = s.m_center.y - ray.O.y;
        const float cz = s.m_center.z - ray.O.z;

        const float t = cx * ray.D.x + cy * ray.D.y + cz * ray.D.z;
        const float qx = cx - t * ray.D.x;
        const float qy = cy - t * ray.D.y;
        const float qz = cz - t * ray.D.z;
        const float p = qx * qx + qy * qy + qz * qz;

        const float r2 = s.m_radius * s.m_radius;
        if (p > r2) return false;

        const float d = sqrtf(r2 - p);
        float hitT = t - d;
        if (hitT <= 0.0f) hitT = t + d;
        if (hitT > 0.0f && hitT < ray.hit.t)
        {
            ray.hit.t = hitT;
            ray.hit.prim = primIdx;
        }

        return true;
    }

    // Callback 3: Occlusion test — return true on any hit closer than ray.hit.t
    static bool sphereIsOccluded(const tinybvh::Ray& ray, const uint32_t primIdx)
    {
        const auto& s = s_kpSpheres[primIdx];
        const float cx = s.m_center.x - ray.O.x;
        const float cy = s.m_center.y - ray.O.y;
        const float cz = s.m_center.z - ray.O.z;

        const float t = cx * ray.D.x + cy * ray.D.y + cz * ray.D.z;
        const float qx = cx - t * ray.D.x;
        const float qy = cy - t * ray.D.y;
        const float qz = cz - t * ray.D.z;
        const float p = qx * qx + qy * qy + qz * qz;

        const float r2 = s.m_radius * s.m_radius;
        if (p > r2) return false;

        const float d = sqrtf(r2 - p);
        float hitT = t - d;
        if (hitT <= 0.0f) hitT = t + d;
        return (hitT > 0.0f && hitT < ray.hit.t);
    }

    // BVHStorage — keeps tinyBVH types off the public header.
    // We still use tinyBVH for *building* the BVH, but traversal
    // is done with our own inlined code to avoid callback overhead.
    struct AnalyticScene::BVHStorage
    {
        tinybvh::BVH m_sphereBlas{};
    };

    // === AnalyticScene lifetime ===

    AnalyticScene::AnalyticScene()
        : m_aSpheres{}, m_pStorage(new BVHStorage())
    { }

    AnalyticScene::~AnalyticScene()
    {
        delete m_pStorage;
        m_pStorage = nullptr;
    }

    uint AnalyticScene::addSphere(const Sphere& s)
    {
        if (m_sphereCount >= kMaxSpheres) return kMaxSpheres;
        const uint idx = m_sphereCount++;
        m_aSpheres[idx] = s;
        return idx;
    }

    void AnalyticScene::removeSphere(const uint idx)
    {
        if (idx >= m_sphereCount) return;
        // Swap-and-pop: move the last sphere into the removed slot
        m_aSpheres[idx] = m_aSpheres[--m_sphereCount];
    }

    void AnalyticScene::setSphereCenter(const uint idx, const float3& center) {
        if (idx < m_sphereCount)
            m_aSpheres[idx].m_center = center;
    }

    void AnalyticScene::clear()
    {
        m_sphereCount = 0;
    }

    // --------------------------------------------------------
    // BVH construction — still uses tinyBVH's builder
    // --------------------------------------------------------

    void AnalyticScene::rebuildSphereBlas() const
    {
        if (m_sphereCount == 0) return;

        s_kpSpheres = m_aSpheres;  // set the static pointer for callbacks

        // Build using custom geometry: pass AABB callback + primitive count
        m_pStorage->m_sphereBlas.Build(sphereBounds, m_sphereCount);

        // Set the intersection callbacks
        m_pStorage->m_sphereBlas.customIntersect = sphereIntersect;
        m_pStorage->m_sphereBlas.customIsOccluded = sphereIsOccluded;
    }

    void AnalyticScene::buildBvh() const
    {
        rebuildSphereBlas();
    }

    // --------------------------------------------------------
    // Inlined BVH traversal with direct sphere intersection
    //
    // This replaces tinyBVH's generic Intersect/IsOccluded which
    // go through function pointer callbacks. By inlining the
    // sphere test directly into the traversal loop, we eliminate:
    //   - function pointer call overhead per leaf primitive
    //   - tinybvh::Ray construction/conversion overhead
    //   - the indirection through a static pointer
    //
    // The traversal structure mirrors tinyBVH's BVH::Intersect:
    //   - Stack-based ordered traversal
    //   - Visit nearest child first, push far child
    //   - At leaf nodes, test ray against spheres directly
    //
    // Reference: tinyBVH BVH::Intersect in tiny_bvh.h
    // --------------------------------------------------------

    bool AnalyticScene::intersect(const core::Ray& ray, HitInfo& hit) const
    {
        if (m_sphereCount == 0) return false;

        if (!std::isfinite(ray.m_o.x) || !std::isfinite(ray.m_o.y) || !std::isfinite(ray.m_o.z) ||
        !std::isfinite(ray.m_d.x) || !std::isfinite(ray.m_d.y) || !std::isfinite(ray.m_d.z))
            return false;

        tinybvh::Ray tbvhRay(
            tinybvh::bvhvec3(ray.m_o.x, ray.m_o.y, ray.m_o.z),
            tinybvh::bvhvec3(ray.m_d.x, ray.m_d.y, ray.m_d.z),
            hit.m_t);

        m_pStorage->m_sphereBlas.Intersect(tbvhRay);

        if (tbvhRay.hit.t >= hit.m_t) return false;

        const uint32_t idx = tbvhRay.hit.prim;
        const Sphere& s = m_aSpheres[idx];
        const float3 ip = ray.m_o + tbvhRay.hit.t * ray.m_d;

        hit.m_t        = tbvhRay.hit.t;
        hit.m_normal   = normalize(ip - s.m_center);
        hit.m_matIndex = s.m_matIndex;
        hit.m_radius   = s.m_radius;
        hit.m_center   = s.m_center;

        return true;
    }

    bool AnalyticScene::isOccluded(const core::Ray& ray) const
    {
        if (m_sphereCount == 0) return false;

        if (!std::isfinite(ray.m_o.x) || !std::isfinite(ray.m_o.y) || !std::isfinite(ray.m_o.z) ||
        !std::isfinite(ray.m_d.x) || !std::isfinite(ray.m_d.y) || !std::isfinite(ray.m_d.z))
            return false;

        tinybvh::Ray tbvhRay(
            tinybvh::bvhvec3(ray.m_o.x, ray.m_o.y, ray.m_o.z),
            tinybvh::bvhvec3(ray.m_d.x, ray.m_d.y, ray.m_d.z),
            ray.m_t);

        return m_pStorage->m_sphereBlas.IsOccluded(tbvhRay);
    }
}