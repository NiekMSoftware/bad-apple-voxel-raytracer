#pragma once

#include "tmpl8/template.h"
#include <string>

namespace rt::core {

    // Equirectangular HDR skydome.
    // Loaded once via Load(); sampled via Sample() with a ray direction.
    // The pixel data is stored as linear floats (no gamma — stbi_loadf preserves HDR).
    //
    // Importance sampling uses the marginal/conditional CDF method:
    //   - Pharr, Jakob & Humphreys, "Physically Based Rendering" (PBRT),
    //     Section 12.6 (Infinite Area Lights) and Section 14.2.4
    //     (Sampling Light Sources: Infinite Area Lights)
    //     https://www.pbr-book.org/3ed-2018/Light_Transport_I_Surface_Reflection/Sampling_Light_Sources#InfiniteAreaLights
    //   - Matt Pharr, "Visualizing Warping Strategies for Sampling Environment Map Lights", 2019
    //     https://pharr.org/matt/blog/2019/06/05/visualizing-env-light-warpings
    class SkyDome
    {
    public:
        SkyDome() = default;
        ~SkyDome();

        // Disable copy
        SkyDome(const SkyDome&)            = delete;
        SkyDome& operator=(const SkyDome&) = delete;

        // Loads a Radiance .hdr equirectangular file.
        // Returns true on success.
        bool load(const std::string& path);

        // Frees pixel data and CDF tables. After this, isLoaded() returns false.
        void unload();

        float3 sample(const float3& direction) const;
        bool isLoaded() const { return m_pPixels != nullptr; }

        // Sample a direction proportional to the HDR luminance.
        // Returns the sampled world-space direction.
        // outPdf receives the probability density (with respect to solid angle).
        static float3 cosineSampleHemisphere(const float3& n, float r1, float r2);

        // --- Importance sampling ---

        // Sample a direction proportional to the HDR luminance.
        // Returns the sampled world-space direction.
        // outPdf receives the probability density (with respect to solid angle).
        float3 sampleImportance(float r1, float r2, float& outPdf) const;

        // Returns the PDF (w.r.t. solid angle) for a given direction.
        // Used for MIS weighting.
        float pdf(const float3& direction) const;

        // Whether the importance sampling tables have been built.
        bool hasImportanceSampling() const { return !m_vAliasProb.empty(); }

        float m_intensity = 1.0f;       // global brightness multiplier

    private:
        float* m_pPixels = nullptr;
        int m_width    = 0;
        int m_height   = 0;
        int m_channels = 0;     // always 3 after load (RGB)

        // --- Importance sampling data (built once after load) ---
        //
        // 2D piecewise-constant distribution using marginal/conditional CDFs.
        // Each row has its own conditional CDF (which column to pick given this row),
        // and the marginal CDF selects which row to sample from.
        //
        // Reference: PBRT 3rd ed., Section 13.6.7 (2D Sampling with Multidimensional Transformations)
        //   https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations

        // Total integrated luminance (for normalization)
        float m_totalLuminance = 0.0f;

        std::vector<float>    m_vAliasProb{};       // size = W * H
        std::vector<uint32_t> m_vAliasIndex{};      // size = W * H
        float m_invTotalLuminance;                  // 1.0 / totalLuminance for PDF calc

        void buildAliasTable();
    };

}  // namespace rt::core