#pragma once

#include "brdf_types.h"
#include "rt/scene/scene.h"

namespace rt::brdf {

    // Convert roughness and anisotropy to anisotropic alpha values
    inline AnisotropicAlpha computeAnisotropicAlpha(float roughness, const float anisotropy)
    {
        // Clamp roughness to avoid numerical issues
        roughness = max(roughness, g_kMinRoughness);

        // Base alpha (Disney convention)
        const float baseAlpha = roughness * roughness;

        // Aspect ratio from anisotropy
        const float aspect = sqrt(1.0f - 0.9f * abs(anisotropy));

        AnisotropicAlpha result{};

        if (anisotropy >= 0.0f) {
            result.m_alphaU = baseAlpha / aspect;     // Larger = more blur along T
            result.m_alphaV = baseAlpha * aspect;     // Smaller = sharper along B
        } else {
            result.m_alphaU = baseAlpha * aspect;     // Smaller = sharper along T
            result.m_alphaV = baseAlpha / aspect;     // Larger = more blur along B
        }

        // Clamp to a safe minimum
        result.m_alphaU = max(result.m_alphaU, g_kMinRoughness);
        result.m_alphaV = max(result.m_alphaV, g_kMinRoughness);

        return result;
    }

    inline float computeEffectiveAlpha(const float3 w, const TangentFrame &frame, const AnisotropicAlpha alpha)
    {
        // Project direction onto tangent plane
        const float tdotW = dot(frame.m_t, w);
        const float bdotW = dot(frame.m_b, w);

        // Weighted combination of alpha values
        // Based on how much the direction aligns with each axis
        float alpha2 = tdotW * tdotW * (alpha.m_alphaU * alpha.m_alphaU) +
            bdotW * bdotW * (alpha.m_alphaV * alpha.m_alphaV);

        // Normalize by projected length squared
        const float projLengthSq = tdotW * tdotW + bdotW * bdotW;

        if (projLengthSq > EPSILON) {
            alpha2 = alpha2 / projLengthSq;
        }

        return max(alpha2, g_kMinRoughness * g_kMinRoughness);
    }

}  // rt::brdf