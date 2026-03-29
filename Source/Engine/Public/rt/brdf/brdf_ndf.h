#pragma once

#include "tmpl8/tmpl8math.h"
#include "tmpl8/common.h"
#include "rt/brdf/brdf_types.h"

namespace rt::brdf {

    // Anisotropic GGX Normal Distribution Function
    // ctx: shading context with pre-computed dot products
    // alpha: anisotropic alpha values
    inline float distributionGgx(const ShadingContext &ctx, const AnisotropicAlpha alpha)
    {
        const float ndotH = ctx.m_nDotH;

        // Below horizon check
        if (ndotH <= 0.0f)
            return 0.0f;

        // Get tangent-space components of H
        const float tdotH = ctx.m_tDotH;
        const float bdotH = ctx.m_bDotH;

        // Get the anisotropic exponent term
        const float ax = alpha.m_alphaU;
        const float ay = alpha.m_alphaV;

        const float term = (tdotH / ax) * (tdotH / ax) +
                     (bdotH / ay) * (bdotH / ay) + ndotH * ndotH;

        // GGX distribution
        const float denom = PI * ax * ay * term * term;
        if (denom < EPSILON)
            return 0.0f;

        return 1.0f / denom;
    }

} // rt::brdf