#pragma once

#include "brdf_types.h"
#include "brdf_fresnel.h"
#include "brdf_ndf.h"
#include "brdf_geometry.h"
#include "brdf_diffuse.h"
#include "rt/rendering/material.h"

namespace rt::brdf {

    inline EvaluationResult evaluate(ShadingContext ctx, rendering::Material material)
    {
        EvaluationResult result{};

        // Early exit for back-facing
        if (ctx.isBackFace())
            return result;

        // Compute anisotropic alpha values
        const AnisotropicAlpha alpha = computeAnisotropicAlpha(material.m_roughness, material.m_anisotropy);

        // Calculate base reflectivity
        const float3 f0 = calculateF0(material.m_baseColor, material.m_metallic, material.m_indexOfRefraction);

        // === SPECULAR ===
        const float d = distributionGgx(ctx, alpha);
        const float g = geometrySmith(ctx, alpha);
        const float3 f = fresnelSchlick(ctx.m_vDotH, f0);

        // Cook-Torrance specular BRDF
        float denom = 4.0f * ctx.m_nDotV * ctx.m_nDotL + EPSILON;
        float3 specular = (d * g / denom) * f;

        // === DIFFUSE ===
        const float3 kS = f;
        const float3 kD = (float3(1.0f) - kS) * (1.0f - material.m_metallic);
        const float3 diffuse = kD * diffuseLambert(material.m_baseColor);

        // Store results
        result.m_diffuse = diffuse;
        result.m_specular = specular;
        result.m_fresnel = f;
        return result;
    }

} // rt::brdf