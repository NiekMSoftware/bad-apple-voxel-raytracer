#pragma once

#include "rt/brdf/brdf.h"
#include "rt/brdf/brdf_types.h"

namespace rt::lights {

    struct LightSample
    {
        float3 m_direction;     // L: unit vector from surface point toward the light
        float3 m_intensity;     // Li: color * power * attenuation (everything pre-computed)
        float  m_distance;      // distance to the light sample point (LARGE_FLOAT for directional)
        bool   m_bIsValid;      // false if the sample contributes nothing (e.g. outside cone)
    };

    inline float3 evaluate(
        const LightSample& sample,
        const float3& I,
        const float3& N,
        const float3& V,
        const rendering::Material& mat,
        const scene::Scene& scene,
        const rendering::MaterialManager& matMgr)
    {
        // early out for zero-radiance samples
        if (!sample.m_bIsValid) return {0};

        // Shadow test
        core::Ray shadowRay(I + N * EPSILON, sample.m_direction, sample.m_distance);
        if (scene.isOccluded(shadowRay, matMgr)) return {0};

        // BSDF evaluation
        brdf::TangentFrame frame = brdf::TangentFrame::fromNormal(N);
        brdf::ShadingContext ctx = brdf::ShadingContext::create(frame, V, sample.m_direction);

        const float3 bsdf = bsdf::evaluate(ctx, mat);
        return bsdf * sample.m_intensity * ctx.m_nDotL;
    }

}  // namespace rt::lights