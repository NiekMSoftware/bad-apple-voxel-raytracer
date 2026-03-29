#include "../../Public/rt/rendering/shading.h"

#include "rt/core/sky_dome.h"
#include "rt/scene/scene.h"
#include "rt/management/light_manager.h"
#include "rt/rendering/material_manager.h"
#include "rt/rendering/simd_utils.h"
#include "rt/brdf/brdf_fresnel.h"
#include "rt/rendering/bit_packer.h"
#include "tmpl8/tmpl8math.h"
#include "tmpl8/template.h"

namespace rt::rendering {

    // =========================================================================
    // IBL helper
    // =========================================================================

    float3 sampleIbl(const float3& biasedI,
                     const float3& N,
                     const ShadingServices& services)
    {
        if (!services.m_bAccumulate) return {0.0f};
        if (!services.m_skyDome.isLoaded()) return {0.0f};

        float envPdf = 0.0f;
        const float3 iblDir = services.m_skyDome.sampleImportance(
            services.m_blueNoiseSample.x,
            services.m_blueNoiseSample.y,
            envPdf);

        const float cosTheta = dot(N, iblDir);
        if (cosTheta <= 0.0f || envPdf <= 0.0f) return {0.0f};

        core::Ray iblRay(biasedI, iblDir, 0.3f);
        if (services.m_scene.isOccluded(iblRay)) return {0.0f};

        const float3 radiance = services.m_skyDome.sample(iblDir);
        return clampFireflySSE(radiance * cosTheta / envPdf * config::g_kIblWeight);
    }

    // =========================================================================
    // Static helpers
    // =========================================================================

    bool survivesRussianRoulette(float3& throughput, const int bounce)
    {
        if (bounce < config::g_kRussianRouletteMinBounce) return true;

        const float pSurvive = min(
            max(max(throughput.x, throughput.y), throughput.z), 1.0f);

        if (RandomFloat() > pSurvive)
            return false;

        throughput *= 1.0f / pSurvive;
        return true;
    }

    float3 beerLambertAbsorption(const float3& baseColor,
                                  const float rayT,
                                  const float primRadius)
    {
        return beerLambertSSE(baseColor, rayT, primRadius);
    }

    // =========================================================================
    // Per-hit shading functions
    // =========================================================================

    BounceResult shadeDielectric(core::Ray& ray,
                                  const float3&  I,
                                  const float3&  N,
                                  const float3&  V,
                                  const Material& mat,
                                  const float maxRayDist,
                                  float3& throughput,
                                  const ShadingServices& services)
    {
        BounceResult result{};
        result.m_bContinue = true;

        const bool   entering = dot(N, V) > 0.0f;
        const float3 faceN    = entering ? N : -N;
        const float3 biasedI  = I + faceN * EPSILON;

        if (mat.m_transparency < 1.0f)
        {
            const float opacity = 1.0f - mat.m_transparency;
            result.m_color = services.m_lightManager.calculateLighting(
                biasedI, faceN, V, mat, services.m_scene, services.m_materialManager,
                services.m_bAccumulate)
                * opacity * opacity;
        }

        const float n1   = entering ? 1.0f : mat.m_indexOfRefraction;
        const float n2   = entering ? mat.m_indexOfRefraction : 1.0f;
        const float eta  = n1 / n2;
        const float cosI = dot(faceN, V);
        const float fresnel = brdf::fresnelSchlickScalar(cosI, n1, n2);

        const float sin2T = eta * eta * (1.0f - cosI * cosI);
        const bool  tir   = (sin2T >= 1.0f);

        const bool reflect = tir || (services.m_bAccumulate
                                         ? RandomFloat() < fresnel
                                         : fresnel > 0.5f);

        if (reflect)
        {
            const float3 reflDir = ray.m_d - 2.0f * dot(ray.m_d, faceN) * faceN;
            ray = core::Ray(biasedI, reflDir, maxRayDist);
        }
        else
        {
            const float oldT          = ray.m_t;
            const float oldPrimRadius = ray.m_primRadius;

            const float  cosT    = sqrtf(1.0f - sin2T);
            const float3 refrDir = eta * ray.m_d + (eta * cosI - cosT) * faceN;
            ray = core::Ray(I - faceN * EPSILON, refrDir, maxRayDist);

            if (!entering)
                throughput *= beerLambertSSE(mat.m_baseColor, oldT, oldPrimRadius);
        }

        return result;
    }

    // ------------------------------------------------------------------------

    BounceResult shadeOpaqueSphere(core::Ray&             ray,
                                   const float3&          I,
                                   const float3&          N,
                                   const float3&          V,
                                   const Material&        mat,
                                   const float            maxRayDist,
                                   const ShadingServices& services)
    {
        BounceResult result{};

        const bool   entering = dot(N, V) > 0.0f;
        const float3 faceN    = entering ? N : -N;
        const float3 biasedI  = I + faceN * EPSILON;

        result.m_color = services.m_lightManager.calculateLighting(
            biasedI, N, V, mat, services.m_scene, services.m_materialManager,
            services.m_bAccumulate);

        if (services.m_bAccumulate &&
            RandomFloat() < config::g_kIblRussianRouletteProbability)
            result.m_color += mat.m_baseColor
                * sampleIbl(biasedI, N, services) * (1.0f / config::g_kIblRussianRouletteProbability);

        if (mat.m_roughness < config::g_kReflectionRoughnessThreshold)
        {
            const float3 f0 = brdf::calculateF0(
                mat.m_baseColor, mat.m_metallic, mat.m_indexOfRefraction);
            const float  cosTheta = max(dot(N, V), 0.0f);
            const float3 f = brdf::fresnelSchlick(cosTheta, f0);

            result.m_throughputScale = f;
            result.m_bContinue       = true;

            const float3 reflDir = ray.m_d - 2.0f * dot(ray.m_d, N) * N;
            ray = core::Ray(biasedI, reflDir, maxRayDist);
        }

        return result;
    }

    // ------------------------------------------------------------------------

    BounceResult shadeVoxel(const core::Ray&       ray,
                            const ShadingServices& services)
    {
        BounceResult result{};

        const float3 I = ray.intersectionPoint();
        const float3 N = ray.getNormal();
        const float3 V = -ray.m_d;

        const uint matID = getMaterialID(ray.m_voxel);
        const Material& material = services.m_materialManager.getMaterial(matID);

        result.m_color = services.m_lightManager.calculateLighting(I, N, V,
            material, services.m_scene, services.m_materialManager,
            services.m_bAccumulate);

        if (services.m_bAccumulate &&
            RandomFloat() < config::g_kIblRussianRouletteProbability)
        {
            const float3 biasedI = I + N * EPSILON;
            result.m_color += ray.getAlbedo()
                * sampleIbl(biasedI, N, services) * (1.0f / config::g_kIblRussianRouletteProbability);
        }

        return result;
    }

    BounceResult shadeHit(core::Ray& ray,
                         const float maxRayDist,
                         float3& throughput,
                         const MaterialManager& matMgr,
                         const ShadingServices& services)
    {
        // ---- Analytic primitive (sphere) ----
        if (ray.m_voxel == 0x40000000u)
        {
            const Material& mat = matMgr.getMaterial(ray.m_primMatIndex);
            const float3 I = ray.intersectionPoint();
            const float3 N = ray.m_primNormal;
            const float3 V = -ray.m_d;

            return (mat.m_transparency > 0.0f)
                ? shadeDielectric(ray, I, N, V, mat, maxRayDist, throughput, services)
                : shadeOpaqueSphere(ray, I, N, V, mat, maxRayDist, services);
        }

        // ---- TLAS instance voxel hit (bit 29 set) ----
        // ray.m_primNormal   = world-space surface normal (set by scene.cpp)
        // ray.m_primMatIndex = material ID from the BLAS voxel
        // ray.getAlbedo()    = RGB from lower 24 bits (still valid with sentinel encoding)
        if ((ray.m_voxel & 0xE0000000u) == 0x20000000u)
        {
            const float3 I = ray.intersectionPoint();
            const float3 N = ray.m_primNormal;
            const float3 V = -ray.m_d;
            const Material& material = matMgr.getMaterial(ray.m_primMatIndex);

            BounceResult result{};
            result.m_color = services.m_lightManager.calculateLighting(
                I + N * EPSILON, N, V, material,
                services.m_scene, services.m_materialManager,
                services.m_bAccumulate);

            if (services.m_bAccumulate &&
                RandomFloat() < config::g_kIblRussianRouletteProbability)
            {
                const float3 biasedI = I + N * EPSILON;
                result.m_color += ray.getAlbedo()
                    * sampleIbl(biasedI, N, services)
                    * (1.0f / config::g_kIblRussianRouletteProbability);
            }

            return result;
        }

        // ---- World voxel (and mesh sentinels) ----
        return shadeVoxel(ray, services);
    }
}  // namespace rt::rendering