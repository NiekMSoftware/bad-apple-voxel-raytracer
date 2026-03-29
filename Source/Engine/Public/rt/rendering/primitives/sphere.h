#pragma once
#include "rt/core/ray.h"
#include "tmpl8/template.h"

namespace rt::primitives {

    struct Sphere 
    {
        float3 m_center;
        float  m_radius;
        uint   m_matIndex = 0;

        void getAabb(float3& min, float3& max) const
        {
            min = m_center - float3(m_radius);
            max = m_center + float3(m_radius);
        }
    };

    inline bool intersectSphere(const core::Ray& ray, const Sphere& sphere, float& t)
    {
        const float3 oc = ray.m_o - sphere.m_center;
        const float a = dot(ray.m_d, ray.m_d);
        const float b = 2.0f * dot(oc, ray.m_d);
        const float c = dot(oc, oc) - sphere.m_radius * sphere.m_radius;

        const float discriminant = b * b - 4 * a * c;
        if (discriminant < 0) return false;

        const float sqrtD = sqrtf(discriminant);
        const float t0 = (-b - sqrtD) / (2 * a);
        const float t1 = (-b + sqrtD) / (2 * a);

        t = (t0 > 0) ? t0 : t1;
        return t > 0;
    }

    inline float intersectSphere(const core::Ray &ray, const Sphere &sphere)
    {
        const float3 center = sphere.m_center - ray.m_o;
        float t = dot(center, ray.m_d);
        const float3 q = center - t * ray.m_d;
        const float p = dot(q, q);

        const float r2 = sphere.m_radius * sphere.m_radius;
        if (p > r2) return 1e34f;

        t -= sqrt(r2 - p);
        return (t > 0) ? t : 1e34f;
    }

}  // namespace Tmpl8::Primitives