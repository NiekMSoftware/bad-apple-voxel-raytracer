#pragma once

#include "rt/core/ray.h"
#include "rt/rendering/material.h"
#include "rt/rendering/config.h"

namespace rt::primitives {
    struct HitInfo;
}

namespace rt::rendering {

    // =========================================================================
    // Per-hit shading functions
    //
    // Each returns a BounceResult:
    //   .m_color          = direct lighting contribution for this hit
    //   .m_throughputScale = how to scale the path throughput going forward
    //   .m_bContinue      = whether the trace loop should keep bouncing
    //
    // If m_bContinue is true, the function overwrites `ray` in-place
    // with the next bounce origin + direction.
    //
    // All dependencies are passed through the ShadingServices struct.
    // =========================================================================

    // Dielectric (glass/water/diamond): stochastic reflect or refract
    [[nodiscard]] BounceResult shadeDielectric(core::Ray&             ray,
                                               const primitives::HitInfo& hitInfo,
                                               const float3&          I,
                                               const float3&          N,
                                               const float3&          V,
                                               const Material&        mat,
                                               float                  maxRayDist,
                                               float3&                throughput,
                                               const ShadingServices& services);

    // Opaque sphere: direct lighting + IBL + optional specular bounce
    [[nodiscard]] BounceResult shadeOpaqueSphere(core::Ray&             ray,
                                                 const float3&          I,
                                                 const float3&          N,
                                                 const float3&          V,
                                                 const Material&        mat,
                                                 float                  maxRayDist,
                                                 const ShadingServices& services);

    // Voxel hit: direct lighting + IBL, no bouncing
    [[nodiscard]] BounceResult shadeVoxel(const core::Ray&       ray,
                                          const ShadingServices& services);

    // =========================================================================
    // IBL helper
    // =========================================================================
    [[nodiscard]] float3 sampleIbl(const float3& biasedI,
                                    const float3& N,
                                    const ShadingServices& services);

    // =========================================================================
    // Russian Roulette
    // Returns false if the path should be terminated.
    // On survival, divides throughput by the survival probability.
    // =========================================================================
    [[nodiscard]] bool survivesRussianRoulette(float3& throughput, int bounce);

    // =========================================================================
    // Beer-Lambert absorption
    // Returns the transmittance for a ray exiting a transparent medium.
    // =========================================================================
    [[nodiscard]] float3 beerLambertAbsorption(const float3& baseColor,
                                                float rayT,
                                                float primRadius);

    BounceResult shadeHit(core::Ray&             ray,
                          const primitives::HitInfo&   hitInfo,
                          float                  maxRayDist,
                          float3&                throughput,
                          const MaterialManager& matMgr,
                          const ShadingServices& services);

}  // namespace rt::rendering