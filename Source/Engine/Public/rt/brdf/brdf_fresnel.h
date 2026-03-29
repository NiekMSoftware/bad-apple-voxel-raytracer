#pragma once

#include "tmpl8/tmpl8math.h"

namespace rt::brdf {

    // Fresnel-Schlick approximation
    //
    // Reference:
    // Fresnel Equation (Other Schlick Approximation)
    // https://ix.cs.uoregon.edu/~hank/441/lectures/pbr_slides.pdf
    inline float3 fresnelSchlick(const float cosTheta, const float3 f0)
    {
        return f0 + (1.0f - f0) * pow5f(1.0f - cosTheta);
    }

    // Calculate F0 (base reflectivity) from IOR or metallic workflow
    inline float3 calculateF0(const float3 baseColor, const float metallic, const float ior)
    {
        // For dielectrics, F0 is derived from IOR
        // F0 = ((n1-n2) / (n1+n2))^2, assuming n1 = 1.0 (air)
        const float dielectricF0 = pow2f((1.0f - ior) / (1.0f + ior));

        // For metals, F0 is the base color itself
        // Blend between dielectric and metallic based on a metallic factor
        return lerp(float3(dielectricF0), baseColor, metallic);
    }

    // Scalar Fresnel for simple cases (e.g., glass)
    inline float fresnelSchlickScalar(const float cosTheta, const float n1, const float n2)
    {
        // Base reflectance at normal incidence
        const float f0 = pow2f((n1 - n2) / (n1 + n2));

        // Approximation
        const float fresnel = f0 + (1.0f - f0) * pow5f(1.0f - cosTheta);
        return fresnel;
    }

    inline float3 fresnelSchlickRoughness(const float cosTheta, const float3 f0, const float roughness)
    {
        const float3 oneMinusRoughness = float3(1.0f - roughness);
        return f0 + (fmaxf(oneMinusRoughness, f0) - f0) * pow5f(1.0f - cosTheta);
    }

}  // rt::brdf