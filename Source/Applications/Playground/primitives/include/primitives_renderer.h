#pragma once

#include "tmpl8/app.h"

#include "rt/core/camera.h"
#include "rt/core/ray.h"
#include "rt/core/sky_dome.h"
#include "rt/scene/scene.h"
#include "rt/rendering/material.h"
#include "rt/rendering/material_manager.h"
#include "rt/management/light_manager.h"
#include "rt/management/ui_manager.h"

namespace Tmpl8::primitives {

    // =========================================================================
    // Renderer-local constants
    // =========================================================================
    namespace config {
        constexpr float g_kBaseSecondaryDist = 0.15f;
        constexpr int   g_kMaxSpp             = 4096;
        constexpr int   g_kMaxDepth           = 4;
        constexpr int   g_kRussianRouletteMinBounce = 2;
        constexpr float g_kMinThroughput      = 0.01f;
        constexpr float g_kIblWeight          = 0.5f;
        constexpr float g_kIblRussianRouletteProbability = 0.10f;
        constexpr float g_kReflectionRoughnessThreshold = 0.3f;
    }

    // =========================================================================
    // BounceResult
    // Returned by each per-hit shading method to tell the trace loop
    // what to do next: terminate or continue with an updated ray.
    // =========================================================================
    struct BounceResult
    {
        float3 m_color{0};              // direct lighting to accumulate this bounce
        float3 m_throughputScale{1.0f}; // multiplier for path throughput (e.g. Fresnel)
        bool   m_bContinue = false;     // true = keep bouncing with updated ray
    };

    // =========================================================================
    // PrimitivesRenderer
    // =========================================================================
    class PrimitivesRenderer : public Application
    {
    public:
        PrimitivesRenderer();

        // Application interface
        void Init()     override;
        void Tick(float deltaTime) override;
        void UI()       override;
        void Shutdown() override;

        // Top-level iterative path tracer
        [[nodiscard]] float3 trace(rt::core::Ray& ray, float maxRayDist = 1e34f) const;

    private:
        // -----------------------------------------------------------------
        // Per-hit shading methods
        //
        // Each returns a BounceResult:
        //   .m_color          = direct lighting contribution for this hit
        //   .m_throughputScale = how to scale the path throughput going forward
        //   .m_bContinue      = whether the loop should keep bouncing
        //
        // If m_bContinue is true, the method overwrites `ray` in-place
        // with the next bounce origin + direction.
        // -----------------------------------------------------------------

        // Dielectric (glass/water/diamond): stochastic reflect or refract
        [[nodiscard]] BounceResult shadeDielectric(rt::core::Ray& ray,
                                                   const float3&  I,
                                                   const float3&  N,
                                                   const float3&  V,
                                                   const rt::rendering::Material& mat,
                                                   int   bounce,
                                                   float maxRayDist,
                                                   float3& throughput) const;

        // Opaque sphere: direct lighting + IBL + optional specular bounce
        [[nodiscard]] BounceResult shadeOpaqueSphere(rt::core::Ray& ray,
                                                     const float3&  I,
                                                     const float3&  N,
                                                     const float3&  V,
                                                     const rt::rendering::Material& mat,
                                                     int   bounce,
                                                     float maxRayDist) const;

        // Voxel hit: direct lighting + IBL, no bouncing
        [[nodiscard]] BounceResult shadeVoxel(const rt::core::Ray& ray,
                                              int bounce) const;

        // -----------------------------------------------------------------
        // IBL helper
        // -----------------------------------------------------------------
        [[nodiscard]] float3 sampleIbl(const float3& biasedI,
                                       const float3& N) const;

        // -----------------------------------------------------------------
        // Russian Roulette
        // Returns false if the path should be terminated.
        // On survival, divides throughput by the survival probability.
        // -----------------------------------------------------------------
        [[nodiscard]] static bool survivesRussianRoulette(float3& throughput, int bounce);

        // -----------------------------------------------------------------
        // Beer-Lambert absorption
        // Returns the transmittance for a ray exiting a transparent medium.
        // -----------------------------------------------------------------
        [[nodiscard]] static float3 beerLambertAbsorption(const float3& baseColor,
                                                          float rayT,
                                                          float primRadius);

        // -----------------------------------------------------------------
        // Accumulator / frame management
        // -----------------------------------------------------------------
        void resetAccumulator();
        void accumulatePixel(int idx, const float3& newSample) const;
        void presentAccumulator() const;

        // -----------------------------------------------------------------
        // Scene construction (called from Init)
        // -----------------------------------------------------------------
        void buildScene();

        // -----------------------------------------------------------------
        // Skydome
        // -----------------------------------------------------------------
        void scanHdrFiles();
        std::vector<std::string> m_hdrFiles;
        int m_selectedHdr = 0;

        // -----------------------------------------------------------------
        // Frame state
        // -----------------------------------------------------------------
        int   m_spp        = 0;
        uint  m_frameIndex = 0;
        float m_exposure = 1.0f;

        float3* m_pAccumulator = nullptr;
        float3* m_pPrevFrame   = nullptr;

        // -----------------------------------------------------------------
        // Scene / rendering objects
        // -----------------------------------------------------------------
        rt::scene::Scene  m_scene;
        rt::core::Camera  m_camera;
        rt::core::SkyDome m_skyDome;

        rt::management::LightManager    m_lightManager;
        rt::rendering::MaterialManager  m_materialManager;
        rt::management::UIManager       m_uiManager;

        rt::management::DiagnosticsData m_lastDiag;
    };

    // == Claude Generated code

        // ============================================================================
        // clampFirefly — SSE version
        //   Original: 4 muls + 1 compare + 1 branch + conditional 3 muls
        //   SIMD:     load-3, dot via dp_ps (4 lanes), compare, blend, store
        // ============================================================================
        inline float3 clampFireflySSE(const float3& c, const float threshold = 10.0f)
        {
            // Load x,y,z into a __m128 (w=0)
            __m128 col = _mm_set_ps(0.0f, c.z, c.y, c.x);

            // Dot product with luminance weights; result broadcast to all lanes
            // _MM_HINT: 0b01110001 = use lanes 0-2 for src, put result in lane 0 only
            __m128 lum = _mm_dp_ps(col, _mm_set_ps(0.0f, 0.0722f, 0.7152f, 0.2126f), 0x71);

            // lum[0] now holds luminance. Compare lum > threshold.
            float lumVal = _mm_cvtss_f32(lum);
            if (lumVal <= threshold) return c;              // fast early-out (no store needed)

            float scale = threshold / lumVal;
            __m128 s = _mm_set1_ps(scale);
            __m128 result = _mm_mul_ps(col, s);

            float buf[4];
            _mm_storeu_ps(buf, result);
            return float3(buf[0], buf[1], buf[2]);
        }

        // ============================================================================
        // beerLambertAbsorption — SSE version
        //   Original: 3x logf + 3x expf (scalar, sequential)
        //   SIMD:     parallel log/exp via approximation (saves ~2/3 of latency)
        //
        //   Uses the fast log2/exp2 trick: loge(x) = log2(x) * ln(2)
        //   Accuracy: ~4 ULP — fine for colour math.
        // ============================================================================
        namespace detail {
            // Fast vectorised log2 (Mineiro's poly, SSE)
            inline __m128 log2_ps(__m128 x)
            {
                __m128i xi = _mm_castps_si128(x);
                // Extract exponent
                __m128i exp = _mm_srli_epi32(_mm_and_si128(xi, _mm_set1_epi32(0x7F800000)), 23);
                exp = _mm_sub_epi32(exp, _mm_set1_epi32(127));
                // Normalise mantissa to [1,2)
                __m128 m = _mm_or_ps(_mm_castsi128_ps(_mm_and_si128(xi, _mm_set1_epi32(0x007FFFFF))),
                                     _mm_set1_ps(1.0f));
                // Polynomial approximation of log2 on [1,2): fits in 4 coefficients
                __m128 p = _mm_set1_ps(-0.34484843f);
                p = _mm_add_ps(_mm_mul_ps(p, m), _mm_set1_ps(2.02466578f));
                p = _mm_add_ps(_mm_mul_ps(p, m), _mm_set1_ps(-0.67487759f));
                p = _mm_sub_ps(p, m);  // final correction term
                return _mm_add_ps(p, _mm_cvtepi32_ps(exp));
            }

            // Fast vectorised exp2 (SSE)
            inline __m128 exp2_ps(__m128 x)
            {
                // Split into integer + fraction
                __m128 xi = _mm_round_ps(x, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC);
                __m128 frac = _mm_sub_ps(x, xi);
                // Integer part as a biased exponent
                __m128i iexp = _mm_add_epi32(_mm_cvtps_epi32(xi), _mm_set1_epi32(127));
                __m128 epow = _mm_castsi128_ps(_mm_slli_epi32(iexp, 23));
                // Polynomial for 2^frac on [0,1)
                __m128 p = _mm_set1_ps(0.00133952f);
                p = _mm_add_ps(_mm_mul_ps(p, frac), _mm_set1_ps(0.00961751f));
                p = _mm_add_ps(_mm_mul_ps(p, frac), _mm_set1_ps(0.05550327f));
                p = _mm_add_ps(_mm_mul_ps(p, frac), _mm_set1_ps(0.24022652f));
                p = _mm_add_ps(_mm_mul_ps(p, frac), _mm_set1_ps(0.69315308f));
                p = _mm_add_ps(_mm_mul_ps(p, frac), _mm_set1_ps(1.0f));
                return _mm_mul_ps(epow, p);
            }
        }

        inline float3 beerLambertSSE(const float3& baseColor, const float rayT, const float primRadius)
        {
            const float chord = rayT / (2.0f * primRadius);
            const float kLn2  = 0.693147f;  // precomputed ln(2)

            // Load and clamp color to [1e-4, inf)
            __m128 col = _mm_max_ps(
                _mm_set_ps(1.0f, baseColor.z, baseColor.y, baseColor.x),
                _mm_set_ps(1.0f, 1e-4f, 1e-4f, 1e-4f));

            // sigma = -log(col) = -log2(col) * ln(2)
            __m128 sig = _mm_mul_ps(
                _mm_sub_ps(_mm_setzero_ps(), detail::log2_ps(col)),
                _mm_set1_ps(kLn2));

            // result = exp(-sigma * chord) = exp2(-sigma * chord / ln(2))
            __m128 neg_sc = _mm_mul_ps(sig, _mm_set1_ps(-chord / kLn2));
            __m128 result = detail::exp2_ps(neg_sc);

            float buf[4];
            _mm_storeu_ps(buf, result);
            return float3(buf[0], buf[1], buf[2]);
        }

        // ============================================================================
        // presentAccumulator — AVX version, processes 8 pixels per iteration
        //   Original: 3 divs + 3 sqrts + 1 pack per pixel (sequential)
        //   SIMD:     8 pixels interleaved via AVX, ~4x throughput on the inner loop
        //
        //   Call this instead of your existing presentAccumulator loop body.
        //   Assumes screen->pixels is uint32_t[SCRWIDTH*SCRHEIGHT].
        // ============================================================================
        inline void presentAccumulatorAVX(const float3* __restrict accum,
                                           uint32_t*    __restrict pixels,
                                           const int                 pixelCount,
                                           const float               exposure)
        {
            const __m256 vExp  = _mm256_set1_ps(exposure);
            const __m256 vOne  = _mm256_set1_ps(1.0f);
            const __m256 vZero = _mm256_setzero_ps();
            const __m256 v255  = _mm256_set1_ps(255.0f);

            int i = 0;
            // Process 8 pixels at a time (but float3 isn't packed, so we need to gather)
            // Strategy: process one channel at a time over 8 consecutive pixels
            for (; i + 8 <= pixelCount; i += 8)
            {
                // Gather R, G, B for 8 pixels (interleaved float3 = stride-3 gather)
                // Manual gather is faster than _mm256_i32gather_ps for small strides
                __m256 r = _mm256_set_ps(accum[i+7].x, accum[i+6].x, accum[i+5].x, accum[i+4].x,
                                          accum[i+3].x, accum[i+2].x, accum[i+1].x, accum[i+0].x);
                __m256 g = _mm256_set_ps(accum[i+7].y, accum[i+6].y, accum[i+5].y, accum[i+4].y,
                                          accum[i+3].y, accum[i+2].y, accum[i+1].y, accum[i+0].y);
                __m256 b = _mm256_set_ps(accum[i+7].z, accum[i+6].z, accum[i+5].z, accum[i+4].z,
                                          accum[i+3].z, accum[i+2].z, accum[i+1].z, accum[i+0].z);

                // Exposure
                r = _mm256_mul_ps(r, vExp);
                g = _mm256_mul_ps(g, vExp);
                b = _mm256_mul_ps(b, vExp);

                // Reinhard: x / (1 + x)
                r = _mm256_div_ps(r, _mm256_add_ps(vOne, r));
                g = _mm256_div_ps(g, _mm256_add_ps(vOne, g));
                b = _mm256_div_ps(b, _mm256_add_ps(vOne, b));

                // Gamma ~2.0 via sqrt, then scale to [0,255]
                r = _mm256_mul_ps(_mm256_sqrt_ps(r), v255);
                g = _mm256_mul_ps(_mm256_sqrt_ps(g), v255);
                b = _mm256_mul_ps(_mm256_sqrt_ps(b), v255);

                // Clamp and pack to uint8 — convert to int, then narrow
                __m256i ri = _mm256_cvtps_epi32(_mm256_min_ps(r, v255));
                __m256i gi = _mm256_cvtps_epi32(_mm256_min_ps(g, v255));
                __m256i bi = _mm256_cvtps_epi32(_mm256_min_ps(b, v255));

                // Pack 8x ARGB into 8x uint32 manually (AVX2 has no 3-channel pack)
                for (int j = 0; j < 8; j++)
                {
                    const uint32_t rv = static_cast<uint32_t>(((int*)&ri)[j]);
                    const uint32_t gv = static_cast<uint32_t>(((int*)&gi)[j]);
                    const uint32_t bv = static_cast<uint32_t>(((int*)&bi)[j]);
                    pixels[i + j] = (rv << 16) | (gv << 8) | bv;
                }
            }

            // Scalar tail
            for (; i < pixelCount; i++)
            {
                const float3 exposed = accum[i] * exposure;
                const float3 mapped  = float3(exposed.x / (1.0f + exposed.x),
                                               exposed.y / (1.0f + exposed.y),
                                               exposed.z / (1.0f + exposed.z));
                const float3 gamma   = float3(sqrtf(mapped.x), sqrtf(mapped.y), sqrtf(mapped.z));
                pixels[i] = RGBF32_to_RGB8(gamma);
            }
        }

    // === End of Claude code ===

}  // namespace Tmpl8::primitives