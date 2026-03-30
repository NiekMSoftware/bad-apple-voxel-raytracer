#pragma once

// ============================================================================
// Unified BSDF - Physically Based Scattering
// ============================================================================
//
// Consolidates the reflective BRDF (Cook-Torrance + Lambert) and
// transmissive BTDF (Snell refraction) into three operations:
//
//   evaluate(ctx, material)  - f(wo, wi) for a given direction pair
//   sample(wo, N, material)  - importance-sample a scattered direction
//   pdf(wo, wi, N, material) - probability density for MIS
//
// REFERENCES:
//   PBRT 4th Ed., Section 9.1  - BSDF Representation
//   PBRT 4th Ed., Section 9.5  - Dielectric BSDF (delta distributions)
//   PBRT 3rd Ed., Section 8.2  - Specular Reflection and Transmission
//
// ============================================================================

#include "bsdf_types.h"
#include "brdf_evaluate.h"
#include "brdf_fresnel.h"
#include "rt/rendering/material.h"
#include "rt/rendering/config.h"

namespace rt::bsdf {

    // Returns true if the direction vector is finite and has non-negligible length.
    //
    // A zero-length direction causes normalize() inside tinybvh::Ray to compute
    // 0/0 = NaN (IEEE 754-2008 §6.2). Once any ray component is NaN, all
    // subsequent arithmetic propagates it (IEEE 754 §6.2: "operations on NaNs
    // propagate NaN").
    inline bool isValidDirection(const float3& d)
    {
        const float len2 = dot(d, d);
        return std::isfinite(len2) && len2 > 1e-12f;
    }

    // =========================================================================
    // evaluate()
    //
    // Returns the BSDF value f(wo, wi) for a given pair of directions.
    // Used by light_sample.h when computing direct lighting contributions.
    //
    // This wraps brdf::evaluate() (Cook-Torrance specular + Lambert diffuse)
    // and applies opacity attenuation for partially transparent materials.
    //
    // The transmissive lobe (specular refraction) is a delta distribution,
    // so by PBRT convention (Section 9.1.2) it returns zero from f() --
    // "there is zero probability that another sampling method will randomly
    // find the direction from a delta distribution."
    //
    // Only the reflective component contributes to direct light sampling.
    // =========================================================================
    inline float3 evaluate(const brdf::ShadingContext& ctx,
                           const rendering::Material& material)
    {
        // Reflective BRDF: Cook-Torrance + Lambert
        const brdf::EvaluationResult brdfResult = brdf::evaluate(ctx, material);
        float3 result = brdfResult.total();

        // For partially transparent materials, attenuate reflective
        // contribution by opacity squared (matching existing shading behavior).
        if (material.m_transparency > 0.0f)
        {
            const float opacity = 1.0f - material.m_transparency;
            result *= opacity * opacity;
        }

        return result;
    }

    inline BSDFSample sample(const float3&  wo,
                             const float3&  N,
                             const rendering::Material& material,
                             const bool     accumulate)
    {
        BSDFSample result{};

        // Indicent direction (towards surface), used in reflection/refraction formulas
        const float3 D = -wo;

        const bool   entering = dot(N, wo) > 0.0f;
        const float3 faceN = entering ? N : -N;

        // -----------------------------------------------------------------
        // Dielectric path (transparency > 0)
        //
        // Fresnel determines the probability of reflection vs. refraction.
        // Total internal reflection forces the reflection path.
        // -----------------------------------------------------------------
        if (material.m_transparency > 0.0f)
        {
            const float n1   = entering ? 1.0f : material.m_indexOfRefraction;
            const float n2   = entering ? material.m_indexOfRefraction : 1.0f;
            const float eta  = n1 / n2;
            const float cosI = dot(faceN, wo);
            const float fresnel = brdf::fresnelSchlickScalar(cosI, n1, n2);

            const float sin2T = eta * eta * (1.0f - cosI * cosI);
            const bool  tir   = (sin2T >= 1.0f);

            // Lobe selection:
            //   accumulate  → stochastic (RandomFloat() < fresnel)
            //   !accumulate → deterministic (fresnel > 0.5)
            const bool reflect = tir || (accumulate
                                             ? RandomFloat() < fresnel
                                             : fresnel > 0.5f);

            if (reflect)
            {
                result.m_wi       = D - 2.0f * dot(D, faceN) * faceN;
                if (!isValidDirection(result.m_wi)) return result; // m_bIsValid stays false
                result.m_value    = float3{1.0f};
                result.m_pdf      = tir ? 1.0f : fresnel;
                result.m_lobe     = LobeType::SpecularReflection;
                result.m_bIsValid = true;
            }
            else
            {
                const float cosT = sqrtf(1.0f - sin2T);
                result.m_wi       = eta * D + (eta * cosI - cosT) * faceN;
                if (!isValidDirection(result.m_wi)) return result; // m_bIsValid stays false
                result.m_value    = float3{1.0f};
                result.m_pdf      = 1.0f - fresnel;
                result.m_lobe     = LobeType::SpecularTransmission;
                result.m_bIsValid = true;
            }

            return result;
        }

        // -----------------------------------------------------------------
        // Opaque specular path (roughness below threshold)
        //
        // Mirror-like reflection weighted by Fresnel.
        // Only fires for smooth enough surfaces; rough surfaces are
        // handled purely through evaluate() in the direct lighting path.
        // -----------------------------------------------------------------
        if (material.m_roughness < rendering::config::g_kReflectionRoughnessThreshold)
        {
            const float3 f0 = brdf::calculateF0(
                material.m_baseColor, material.m_metallic, material.m_indexOfRefraction);
            const float cosTheta = max(dot(N, wo), 0.0f);
            const float3 f = brdf::fresnelSchlick(cosTheta, f0);

            result.m_wi       = D - 2.0f * dot(D, N) * N;
            if (!isValidDirection(result.m_wi)) return result; // m_bIsValid stays false
            result.m_value    = f;
            result.m_pdf      = 1.0f;
            result.m_lobe     = LobeType::SpecularReflection;
            result.m_bIsValid = true;
            return result;
        }

        // -----------------------------------------------------------------
        // Diffuse only - no bounce
        // All contribution comes from evaluate() in the light sampling path.
        // -----------------------------------------------------------------
        return result;  // m_bIsValid = false
    }

    // =========================================================================
    // pdf()
    //
    // Returns the probability density for sampling wi given wo.
    //
    // For delta distributions (perfect specular reflection/transmission),
    // this returns 0.0 by convention (PBRT 4th Ed., Section 9.1.2):
    // "the respective Pdf() methods return 0 for all directions, since
    // there is zero probability that another sampling method will randomly
    // find the direction from a delta distribution."
    //
    // This becomes non-trivial when rough refraction (microfacet BTDF)
    // or GGX importance sampling is added.
    // =========================================================================
    inline float pdf(const float3& /*wo*/,
                     const float3& /*wi*/,
                     const float3& /*N*/,
                     const rendering::Material& /*material*/)
    {
        // All current lobes are delta distributions.
        return 0.0f;
    }

}  // namespace rt::bsdf