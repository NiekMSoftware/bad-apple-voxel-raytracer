#pragma once

#include "brdf_aniso.h"
#include "tmpl8/tmpl8math.h"

namespace rt::brdf {

    // Schlick-GGX Geometry function
    //
    // Reference:
    // Schlick GGX Geometry Approximation
    // https://ix.cs.uoregon.edu/~hank/441/lectures/pbr_slides.pdf
    inline float geometrySchlickGgx(const float nDotV, const float roughness)
    {
        // remapping for direct lighting
        const float r = roughness + 1.0f;
        const float k = (r * r) / 8.0f;

        const float numerator = nDotV;
        const float denominator = nDotV * (1.0f - k) + k;

        return numerator / denominator;
    }

    inline float lambda(const float3 w, const TangentFrame &frame, const AnisotropicAlpha alpha)
    {
        const float ndotW = abs(dot(frame.m_n, w));

        // Prevent division by zero at grazing angles
        if (ndotW < EPSILON)
            return 0.0f;

        // Compute tan2Theta
        const float ndotWSq = ndotW * ndotW;
        const float tan2Theta = (1.0f - ndotWSq) / ndotWSq;

        // Handle a perfectly perpendicular case
        if (tan2Theta <= 0.0f)
            return 0.0f;

        // Get effective alpha for this direction
        const float effAlpha = computeEffectiveAlpha(w, frame, alpha);

        // GGX Lambda
        const float alpha2Tan2Theta = effAlpha * tan2Theta;

        return (-1.0f + sqrtf(1.0f + alpha2Tan2Theta)) / 2.0f;
    }

    inline float geometrySmith(const ShadingContext &ctx, const AnisotropicAlpha alpha)
    {
        const float lamdaV = lambda(ctx.m_v, ctx.m_frame, alpha);
        const float lamdaL = lambda(ctx.m_l, ctx.m_frame, alpha);
        return 1.0f / (1.0f + lamdaV + lamdaL);
    }

} // rt::brdf