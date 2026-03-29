#pragma once

#include "rt/core/ray.h"

inline float intersectCube(rt::core::Ray& ray)
{
    const float tx1 = -ray.m_o.x * ray.m_rD.x, tx2 = (1.f - ray.m_o.x) * ray.m_rD.x;
    float tmin = min(tx1, tx2), tmax = max(tx1, tx2);

    const float ty1 = -ray.m_o.y * ray.m_rD.y, ty2 = (1.f - ray.m_o.y) * ray.m_rD.y;
    const float ty  = min(ty1, ty2);
    tmin = max(tmin, ty);
    tmax = min(tmax, max(ty1, ty2));

    const float tz1 = -ray.m_o.z * ray.m_rD.z, tz2 = (1.f - ray.m_o.z) * ray.m_rD.z;
    const float tz  = min(tz1, tz2);
    tmin = max(tmin, tz);
    tmax = min(tmax, max(tz1, tz2));

    if      (tmin == tz) ray.m_axis = 2;
    else if (tmin == ty) ray.m_axis = 1;

    return (tmax >= tmin && tmin > 0.f) ? tmin : 1e34f;
}

inline bool pointInCube(const float3& p)
{
    return p.x >= 0.f && p.y >= 0.f && p.z >= 0.f &&
           p.x <= 1.f && p.y <= 1.f && p.z <= 1.f;
}