#pragma once

#include "tmpl8/tmpl8math.h"

// Forward declarations - avoids pulling in full headers
namespace rt::management { class LightManager; }
namespace rt::scene      { class Scene; }
namespace rt::rendering  { class MaterialManager; }
namespace rt::core       { class SkyDome; struct BlueNoise; }

namespace rt::rendering {

    // =========================================================================
    // Path tracer constants
    // =========================================================================
    namespace config {
        constexpr float g_kBaseSecondaryDist             = 0.15f;
        constexpr int   g_kMaxSpp                        = 4096;
        constexpr int   g_kMaxDepth                      = 4;
        constexpr int   g_kRussianRouletteMinBounce      = 2;
        constexpr float g_kMinThroughput                 = 0.01f;
        constexpr float g_kIblWeight                     = 0.5f;
        constexpr float g_kIblRussianRouletteProbability = 0.5f;
        constexpr float g_kReflectionRoughnessThreshold  = 0.15f;
    }

    // =========================================================================
    // BounceResult
    // Returned by each per-hit shading function to tell the trace loop
    // what to do next: terminate or continue with an updated ray.
    // =========================================================================
    struct BounceResult
    {
        float3 m_color{0};              // direct lighting to accumulate this bounce
        float3 m_throughputScale{1.0f}; // multiplier for path throughput (e.g. Fresnel)
        bool   m_bContinue = false;     // true = keep bouncing with updated ray
    };

    // =========================================================================
    // ShadingServices
    // Read-only view of per-frame rendering state, passed to all shading
    // functions so they don't need 4+ individual reference parameters.
    //
    // Constructed once by the path tracer; references are stable for the
    // lifetime of the owning object.
    // =========================================================================
    struct ShadingServices
    {
        const management::LightManager&   m_lightManager;
        const scene::Scene&               m_scene;
        const rendering::MaterialManager& m_materialManager;
        const core::SkyDome&              m_skyDome;

        float2 m_blueNoiseSample = { 0.5f, 0.5f };
        bool m_bAccumulate = false;
    };

}  // namespace rt::rendering