#include "rt/core/sky_dome.h"
#include "stb_image.h"

namespace rt::core {
    SkyDome::~SkyDome()
    {
        if (m_pPixels) {
            stbi_image_free(m_pPixels);
            m_pPixels = nullptr;
        }
    }

    bool SkyDome::load(const std::string& path)
    {
        // Free previous HDR data before loading new one
        if (m_pPixels) {
            stbi_image_free(m_pPixels);
            m_pPixels = nullptr;
        }

        m_pPixels = stbi_loadf(path.c_str(), &m_width, &m_height, &m_channels, 3);
        if (!m_pPixels) {
            printf("SkyDome: failed to load '%s'\n", path.c_str());
            return false;
        }

        // Force channel count to 3
        m_channels = 3;
        printf("SkyDome: loaded '%s' (%dx%d)\n", path.c_str(), m_width, m_height);

        // buildImportanceSamplingData();
        buildAliasTable();
        return true;
    }

    void SkyDome::unload() {
        if (m_pPixels) {
            stbi_image_free(m_pPixels);
            m_pPixels = nullptr;
        }
        m_width          = 0;
        m_height         = 0;
        m_channels       = 0;
        m_totalLuminance = 0.0f;
        m_vAliasIndex.clear();
        m_vAliasProb.clear();
    }

    float3 SkyDome::sample(const float3& direction) const
    {
        if (!m_pPixels) return {0.0f};

        // Convert normalized direction to equirectangular UV
        const float u = atan2f(direction.z, direction.x) * INV2PI + 0.5f;
        const float v = 0.5f - asinf(clamp(direction.y, -1.0f, 1.0f)) * INVPI;

        // Nearest-neighbor pixel lookup (bilinear is optional, but improves quality)
        // Reference: https://jacco.ompf2.com/2022/05/27/how-to-build-a-bvh-part-8-whitted-style/
        const int px = static_cast<int>(u * static_cast<float>(m_width)) % m_width;
        const int py = static_cast<int>(v * static_cast<float>(m_height)) % m_height;

        const int idx = (py * m_width + px) * m_channels;
        return float3(m_pPixels[idx], m_pPixels[idx + 1], m_pPixels[idx + 2]) * m_intensity;
    }

    float3 SkyDome::cosineSampleHemisphere(const float3& n, const float r1, const float r2)
    {
        // Sample unit disk
        const float r     = sqrtf(r1);
        const float theta = 2.0f * PI * r2;
        const float x     = r * cosf(theta);
        const float y     = r * sinf(theta);
        const float z     = sqrtf(max(0.0f, 1.0f - r1));  // project up

        // Build orthonormal basis around N (same as TangentFrame::fromNormal)
        const float3 helper = (fabsf(n.x) > 0.9f) ? float3(0, 1, 0) : float3(1, 0, 0);
        const float3 T = normalize(cross(n, helper));
        const float3 B = cross(n, T);

        // Transform from local hemisphere space to world space
        return T * x + B * y + n * z;
    }

    void SkyDome::buildAliasTable()
    {
        const int N = m_width * m_height;
        m_vAliasProb.resize(N);
        m_vAliasIndex.resize(N);

        // 1. Compute per-pixel weights (luminance * sinTheta)
        std::vector<float> weights(N);
        float totalWeight = 0.0f;
        for (int y = 0; y < m_height; y++)
        {
            const float theta = PI * (static_cast<float>(y) + 0.5f) / static_cast<float>(m_height);
            const float sinTheta = sinf(theta);
            for (int x = 0; x < m_width; x++)
            {
                const int idx = (y * m_width + x) * 3;
                const float lum = 0.2126f * m_pPixels[idx]
                                + 0.7152f * m_pPixels[idx + 1]
                                + 0.0722f * m_pPixels[idx + 2];
                weights[y * m_width + x] = lum * sinTheta;
                totalWeight += lum * sinTheta;
            }
        }

        m_totalLuminance = totalWeight;
        m_invTotalLuminance = (totalWeight > 0.0f) ? 1.0f / totalWeight : 0.0f;

        // 2. Normalize so average = 1.0
        const float scale = static_cast<float>(N) / totalWeight;
        for (int i = 0; i < N; i++)
            weights[i] *= scale;

        // 3. Vose's alias method construction
        std::vector<int> small, large;
        small.reserve(N);
        large.reserve(N);

        for (int i = 0; i < N; i++)
        {
            if (weights[i] < 1.0f)
                small.push_back(i);
            else
                large.push_back(i);
        }

        while (!small.empty() && !large.empty())
        {
            const int s = small.back(); small.pop_back();
            const int l = large.back(); large.pop_back();

            m_vAliasProb[s]  = weights[s];
            m_vAliasIndex[s] = static_cast<uint32_t>(l);

            weights[l] = (weights[l] + weights[s]) - 1.0f;

            if (weights[l] < 1.0f)
                small.push_back(l);
            else
                large.push_back(l);
        }

        // Remaining entries get probability 1.0 (numerical cleanup)
        for (const int i : large) m_vAliasProb[i] = 1.0f;
        for (const int i : small) m_vAliasProb[i] = 1.0f;
    }

    float3 SkyDome::sampleImportance(const float r1, const float r2, float& outPdf) const
    {
        outPdf = 0.0f;
        if (m_vAliasProb.empty()) return {0, 1, 0};

        const int N = m_width * m_height;

        const int   idx  = min(static_cast<int>(r1 * static_cast<float>(N)), N - 1);
        const int sampledIdx = (r2 < m_vAliasProb[idx])
                               ? idx
                               : static_cast<int>(m_vAliasIndex[idx]);

        const int x = sampledIdx % m_width;
        const int y = sampledIdx / m_width;

        // Convert (x, y) to direction — same as before
        const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(m_width);
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(m_height);

        const float phi   = (u - 0.5f) * 2.0f * PI;
        const float theta = v * PI;

        const float sinTheta = sinf(theta);
        const float cosTheta = cosf(theta);

        const float3 direction = float3(
            cosf(phi) * sinTheta,
            -cosTheta,
            sinf(phi) * sinTheta
        );

        // PDF computation — same formula
        if (sinTheta > 0.0f && m_totalLuminance > 0.0f)
        {
            const int pixIdx = (y * m_width + x) * 3;
            const float luminance = 0.2126f * m_pPixels[pixIdx]
                                  + 0.7152f * m_pPixels[pixIdx + 1]
                                  + 0.0722f * m_pPixels[pixIdx + 2];

            outPdf = (luminance * static_cast<float>(m_width * m_height))
                   / (m_totalLuminance * 2.0f * PI * PI);
        }

        return normalize(direction);
    }

    float SkyDome::pdf(const float3& direction) const
    {
        if (!hasImportanceSampling() || m_totalLuminance == 0.0f) return 0.0f;

        // Convert direction to UV
        const float u = atan2f(direction.z, direction.x) * INV2PI + 0.5f;
        const float v = 0.5f - asinf(clamp(direction.y, -1.0f, 1.0f)) * INVPI;

        const int px = clamp(static_cast<int>(u * static_cast<float>(m_width)), 0, m_width - 1);
        const int py = clamp(static_cast<int>(v * static_cast<float>(m_height)), 0, m_height - 1);

        const int idx = (py * m_width + px) * m_channels;
        const float luminance = 0.2126f * m_pPixels[idx]
                              + 0.7152f * m_pPixels[idx + 1]
                              + 0.0722f * m_pPixels[idx + 2];

        return (luminance * static_cast<float>(m_width) * static_cast<float>(m_height))
            / (m_totalLuminance * 2.0f * PI * PI);
    }

}  // namespace rt::core