#pragma once

#include "tmpl8/tmpl8math.h"

namespace rt::brdf {

    constexpr float g_kMinRoughness = 0.04f;  // prevents NaN in GGX

    // Result of BRDF evaluation
    struct EvaluationResult 
    {
        float3 m_diffuse{0};         // diffuse contribution
        float3 m_specular{0};        // specular contribution
        float3 m_fresnel{0};         // fresnel term (for energy conservation)

        [[nodiscard]] float3 total() const {
            return m_diffuse + m_specular;
        }
    };

    // Stores the two alpha values for anisotropic distributions
    struct AnisotropicAlpha 
    {
        float m_alphaU;       // Alpha along the tangent direction
        float m_alphaV;       // Alpha along the bitangent direction
    };

    // Orthonormal basis for surface shading
    struct TangentFrame 
    {
        float3 m_n;       // Normal
        float3 m_t;       // Tangent
        float3 m_b;       // Bitangent

        // Build frame from normal only (procedural tangents)
        static TangentFrame fromNormal(const float3 normal) {
            TangentFrame frame{};
            frame.m_n = normal;

            // Choose a helper vector not parallel to N
            float3 helper{};
            if ( abs(normal.x) > 0.9f ) {
                helper = float3(0, 1, 0);
            } else {
                helper = float3(1, 0, 0);
            }

            frame.m_t = normalize(cross(normal, helper));
            frame.m_b = cross(frame.m_n, frame.m_t);

            return frame;
        }
    };

    // Pre-computed dot products to avoid redundant calculations
    struct ShadingContext 
    {
        TangentFrame m_frame;

        float3 m_v;       // View direction
        float3 m_l;       // Light direction
        float3 m_h;       // Half vector

        float m_nDotV;
        float m_nDotL;
        float m_nDotH;
        float m_vDotH;

        // Tangent-space projections of H (needed for anisotropic D)
        float m_tDotH;
        float m_bDotH;

        // Factory method
        static ShadingContext create(const TangentFrame &f, const float3& v, const float3& l)
        {
            ShadingContext ctx{};
            ctx.m_frame = f;
            ctx.m_v = v;
            ctx.m_l = l;
            ctx.m_h = normalize(l + v);

            ctx.m_nDotV = max(dot(f.m_n, v), EPSILON);
            ctx.m_nDotL = max(dot(f.m_n, l), 0.0f);
            ctx.m_nDotH = max(dot(f.m_n, ctx.m_h), 0.0f);
            ctx.m_vDotH = max(dot(v, ctx.m_h), 0.0f);

            ctx.m_tDotH = dot(f.m_t, ctx.m_h);
            ctx.m_bDotH = dot(f.m_b, ctx.m_h);

            return ctx;
        }

        [[nodiscard]] bool isBackFace() const
        {
            return m_nDotL <= 0.0f;
        }
    };

}  // rt::brdf