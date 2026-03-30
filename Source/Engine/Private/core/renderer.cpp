#include "rt/core/renderer.h"

#include "rt/rendering/shading.h"
#include "rt/rendering/simd_utils.h"
#include "tmpl8/template.h"

// ============================================================================
// YCoCg color space helpers
// Reference:
//   Karis, "High Quality Temporal Supersampling", SIGGRAPH 2014
//   Salvi, "An Excursion in Temporal Supersampling", GDC 2016
// ============================================================================
inline float3 rgbToYCoCg(const float3& rgb)
{
    return {
        0.25f * rgb.x + 0.5f * rgb.y + 0.25f * rgb.z,
        0.5f  * rgb.x                 - 0.5f  * rgb.z,
       -0.25f * rgb.x + 0.5f * rgb.y - 0.25f * rgb.z
    };
}

inline float3 yCoCgToRgb(const float3& ycocg)
{
    const float y  = ycocg.x;
    const float co = ycocg.y;
    const float cg = ycocg.z;
    return { y + co - cg, y + cg, y - co - cg };
}

namespace rt::core {

    // =========================================================================
    // Constructor / Destructor
    // =========================================================================

    Renderer::Renderer()
        : m_skyDome()
        , m_shadingServices{m_lightManager, m_scene, m_materialManager, m_skyDome}
    {}

    Renderer::~Renderer()
    {
        shutdown();
    }

    // =========================================================================
    // Buffer management
    // =========================================================================

    void Renderer::allocateBuffers()
    {
        const int n = m_renderWidth * m_renderHeight;
        m_pPixelBuffer  = new uint32_t[n];
        m_pSampleCount  = new int[n];
        m_pAccumulator  = new float3[n];
        m_pNewSamples   = new float3[n];
        m_pHistory      = new float3[n];
        m_pHistoryDepth = new float[n];
        m_pDepth        = new float[n];
        m_pRayDir       = new float3[n];
    }

    void Renderer::freeBuffers()
    {
        delete[] m_pPixelBuffer;  m_pPixelBuffer  = nullptr;
        delete[] m_pSampleCount;  m_pSampleCount  = nullptr;
        delete[] m_pAccumulator;  m_pAccumulator  = nullptr;
        delete[] m_pNewSamples;   m_pNewSamples   = nullptr;
        delete[] m_pHistory;      m_pHistory      = nullptr;
        delete[] m_pHistoryDepth; m_pHistoryDepth = nullptr;
        delete[] m_pDepth;        m_pDepth        = nullptr;
        delete[] m_pRayDir;       m_pRayDir       = nullptr;
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================

    void Renderer::initialize()
    {
        m_noise.load("assets/LDR_RG01_0.png");
        m_materialManager.initialize();

        // Push the default render resolution into the camera so that
        // getPrimaryRay() uses the correct dimensions from the first frame.
        m_camera.setRenderResolution(m_renderWidth, m_renderHeight);

        allocateBuffers();
        resetAccumulator();
    }

    void Renderer::shutdown()
    {
        freeBuffers();
    }

    void Renderer::setRenderResolution(const int w, const int h)
    {
        if (w == m_renderWidth && h == m_renderHeight) return;
        m_renderWidth  = w;
        m_renderHeight = h;

        // Camera must be updated first — allocateBuffers() uses m_renderWidth/H.
        m_camera.setRenderResolution(w, h);

        freeBuffers();
        allocateBuffers();
        resetAccumulator();
    }

    // =========================================================================
    // Accumulator
    // =========================================================================

    void Renderer::resetAccumulator()
    {
        const int n = m_renderWidth * m_renderHeight;
        memset(m_pSampleCount,  0, static_cast<size_t>(n) * sizeof(int));
        memset(m_pAccumulator,  0, static_cast<size_t>(n) * sizeof(float3));
        memset(m_pNewSamples,   0, static_cast<size_t>(n) * sizeof(float3));
        memset(m_pHistory,      0, static_cast<size_t>(n) * sizeof(float3));
        memset(m_pHistoryDepth, 0, static_cast<size_t>(n) * sizeof(float));
        memset(m_pDepth,        0, static_cast<size_t>(n) * sizeof(float));
        m_spp        = 0;
        m_frameIndex = 0;
    }

    void Renderer::accumulatePixel(const int idx, const float3& newSample) const
    {
        m_pSampleCount[idx]++;
        const int n = m_pSampleCount[idx];

        if (n == 1) { m_pAccumulator[idx] = newSample; return; }

        const float wNew = 1.0f / static_cast<float>(n);
        m_pAccumulator[idx] = newSample * wNew + m_pAccumulator[idx] * (1.0f - wNew);
    }

    void Renderer::presentAccumulator() const
    {
        presentAccumulatorAVX(m_pAccumulator, m_pPixelBuffer,
                               m_renderWidth * m_renderHeight, m_exposure);
    }

    // =========================================================================
    // Trace
    // =========================================================================

    float3 Renderer::trace(Ray& ray, const float maxRayDist, float* primaryDepthOut) const
    {
        float3 color{0};
        float3 throughput{1.0f};

        for (int bounce = 0; bounce < rendering::config::g_kMaxDepth; bounce++)
        {
            if (!rendering::survivesRussianRoulette(throughput, bounce)) {
                color += throughput * sampleSky(ray, bounce);
                break;
            }

            ray.m_t = fminf(ray.m_t, maxRayDist);
            m_scene.findNearest(ray);

            if (bounce == 0 && primaryDepthOut)
                *primaryDepthOut = (ray.m_voxel != 0) ? ray.m_t : LARGE_FLOAT;

            if (ray.m_voxel == 0) {
                color += throughput * sampleSky(ray, bounce);
                break;
            }

            const auto [m_color, m_throughputScale, m_bContinue] = rendering::shadeHit(
                ray, maxRayDist, throughput, m_materialManager, m_shadingServices);

            color += throughput * m_color;
            if (!m_bContinue) break;
            throughput *= m_throughputScale;
        }

        return color;
    }

    float3 Renderer::sampleSky(const Ray& ray, const int bounce) const
    {
        if (!m_skyDome.isLoaded()) return {0};
        float3 skyColor = m_skyDome.sample(ray.m_d);
        if (bounce > 0) skyColor = clampFireflySSE(skyColor);
        return skyColor;
    }

    // =========================================================================
    // renderFrame
    // =========================================================================

    void Renderer::renderFrame()
    {
        const Timer t;

        m_shadingServices.m_bAccumulate = m_bAccumulate;

        const int   width       = m_renderWidth;
        const int   height      = m_renderHeight;
        const float posDelta    = length(m_camera.m_camPos    - m_prevCamPos);
        const float tgtDelta    = length(m_camera.m_camTarget - m_prevCamTarget);
        const bool  cameraMoved = (posDelta > 1e-6f || tgtDelta > 1e-6f);

        // When the camera stops moving, seed the accumulator from the last
        // reprojected output so convergence resumes from a clean base rather
        // than blending against stale reprojection data from the moving frames.
        if (!cameraMoved && m_bPrevCameraMoved)
        {
            const int n = width * height;
            memcpy(m_pAccumulator, m_pHistory, static_cast<size_t>(n) * sizeof(float3));
            memset(m_pSampleCount, 0,          static_cast<size_t>(n) * sizeof(int));
            m_spp = 1;
        }
        m_bPrevCameraMoved = cameraMoved;

        auto reproject = [&](const float3& P, float& prevX, float& prevY) -> bool
        {
            const float3 pR  = float3(m_prevCamToWorld.cell[0], m_prevCamToWorld.cell[4], m_prevCamToWorld.cell[8]);
            const float3 pU  = float3(m_prevCamToWorld.cell[1], m_prevCamToWorld.cell[5], m_prevCamToWorld.cell[9]);
            const float3 pFw = float3(m_prevCamToWorld.cell[2], m_prevCamToWorld.cell[6], m_prevCamToWorld.cell[10]);
            const float3 pPs = float3(m_prevCamToWorld.cell[3], m_prevCamToWorld.cell[7], m_prevCamToWorld.cell[11]);

            const float3 delta  = P - pPs;
            const float3 localP = float3(dot(pR, delta), dot(pU, delta), dot(pFw, delta));
            if (localP.z <= 0.0f) return false;

            const float scale = tanf(m_camera.m_fovDegrees * 0.5f * (PI / 180.0f));
            prevX = ((localP.x / localP.z / (m_camera.m_aspect * scale)) + 1.0f)
                    * 0.5f * static_cast<float>(width)  - 0.5f;
            prevY = (1.0f - localP.y / localP.z / scale)
                    * 0.5f * static_cast<float>(height) - 0.5f;
            return true;
        };

        // ===== PASS 1: Trace all pixels =====
        constexpr int TILE_SIZE = 8;   // 8x8 tiles; must be a power of 2
        const int tilesX = (width  + TILE_SIZE - 1) / TILE_SIZE;
        const int tilesY = (height + TILE_SIZE - 1) / TILE_SIZE;
        const int tileCount = tilesX * tilesY;

#pragma omp parallel for schedule(dynamic)
        for (int tileIdx = 0; tileIdx < tileCount; tileIdx++)
        {
            const int tileX = (tileIdx % tilesX) * TILE_SIZE;
            const int tileY = (tileIdx / tilesX) * TILE_SIZE;

            for (int y = tileY; y < min(tileY + TILE_SIZE, height); y++)
            {
                for (int x = tileX; x < min(tileX + TILE_SIZE, width);  x++)
                {
                    const int idx = x + y * width;

                    // Trace all pixels on the first frame after a reset
                    // after that, alternate checkerboard halves each frame.
                    const bool traceThisPixel = (m_spp == 0) ||
                        (((x + y) & 1) == (static_cast<int>(m_frameIndex) & 1));

                    if (traceThisPixel) {
                        const float2 jitter = m_noise.sample(x, y, m_frameIndex);
                        Ray r = m_camera.getPrimaryRay(
                            static_cast<float>(x) + jitter.x,
                            static_cast<float>(y) + jitter.y);
                        m_shadingServices.m_blueNoiseSample = m_noise.sample(x, y, m_frameIndex + 1);
                        m_pRayDir[idx]      = r.m_d;
                        float primaryDepth  = LARGE_FLOAT;
                        m_pNewSamples[idx]  = trace(r, 1e34f, &primaryDepth);
                        m_pNewSamples[idx]  = clampFireflySSE(m_pNewSamples[idx]);
                        m_pDepth[idx]       = primaryDepth;
                    }
                    else {
                        // Feed last blended result into the reprojector as-is.
                        // Depth is invalidated so the reprojector skips the
                        // world-space lookup and falls back to the sample itself.
                        m_pNewSamples[idx] = m_pHistory[idx];
                        m_pDepth[idx]      = LARGE_FLOAT;
                    }
                }
            }
        }

        // ===== PASS 2: Accumulate / reproject+clip+blend =====
#pragma omp parallel for schedule(dynamic)
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                const int    idx    = x + y * width;
                const float3 sample = m_pNewSamples[idx];

                if (!m_bAccumulate) {
                    m_pAccumulator[idx] = sample;
                    continue;
                }

                if (!cameraMoved) {
                    const bool wasTraced = (m_spp == 0)
                        || (((x + y) & 1) == (static_cast<int>(m_frameIndex) & 1));

                    if (wasTraced)
                        accumulatePixel(idx, sample);
                    else
                        m_pAccumulator[idx] = m_pHistory[idx];  // hold value, do not increment counter

                    continue;
                }

                // --- Reproject ---
                float3          historySample{0};

                // Adapt blend weight based on how much the new sample differs from history.
                // Large difference = disocclusion or fast movement = trust history less.
                const float3 diff   = sample - historySample;
                const float  change = dot(diff, diff);
                const float  w      = change > 0.1f ? 0.5f : 0.4f;

                if (const float depth = m_pDepth[idx]; depth < LARGE_FLOAT)
                {
                    const float3 P = m_camera.m_camPos + depth * m_pRayDir[idx];
                    float prevX = -1.0f, prevY = -1.0f;

                    if (reproject(P, prevX, prevY) &&
                        prevX >= 0 && prevX < static_cast<float>(width  - 2) &&
                        prevY >= 0 && prevY < static_cast<float>(height - 2))
                    {
                        const float fx = prevX - floorf(prevX);
                        const float fy = prevY - floorf(prevY);
                        const int   a  = static_cast<int>(prevX)
                                       + static_cast<int>(prevY) * width;

                        historySample = m_pHistory[a]             * (1-fx) * (1-fy)
                                      + m_pHistory[a + 1]         * fx     * (1-fy)
                                      + m_pHistory[a + width]     * (1-fx) * fy
                                      + m_pHistory[a + width + 1] * fx     * fy;

                        // Variance clipping (Salvi 2016)
                        float3 colorAvg = rgbToYCoCg(sample);
                        float3 colorVar = colorAvg * colorAvg;
                        int    count    = 1;

                        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
                        {
                            if (dx == 0 && dy == 0) continue;
                            const int nx = x + dx, ny = y + dy;
                            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;

                            // Skip neighbours on the opposite checkerboard phase, they contain
                            // stale history values, not fresh samples, which corrupts the variance box
                            if (((nx + ny) & 1) != ((x + y) & 1)) continue;

                            const float3 f = rgbToYCoCg(m_pNewSamples[nx + ny * width]);
                            colorAvg = colorAvg + f;
                            colorVar = colorVar + f * f;
                            count++;
                        }

                        const float invN = 1.0f / static_cast<float>(count);
                        colorAvg *= invN;
                        colorVar *= invN;

                        constexpr float kBoxSigma = 0.75f;
                        const float3 sigma = float3(
                            sqrtf(max(0.0f, colorVar.x - colorAvg.x * colorAvg.x)),
                            sqrtf(max(0.0f, colorVar.y - colorAvg.y * colorAvg.y)),
                            sqrtf(max(0.0f, colorVar.z - colorAvg.z * colorAvg.z)));

                        float3 hY = rgbToYCoCg(historySample);
                        hY = float3(
                            clamp(hY.x, colorAvg.x - kBoxSigma * sigma.x, colorAvg.x + kBoxSigma * sigma.x),
                            clamp(hY.y, colorAvg.y - kBoxSigma * sigma.y, colorAvg.y + kBoxSigma * sigma.y),
                            clamp(hY.z, colorAvg.z - kBoxSigma * sigma.z, colorAvg.z + kBoxSigma * sigma.z));
                        historySample = yCoCgToRgb(hY);
                        historySample = float3(max(0.0f, historySample.x),
                                               max(0.0f, historySample.y),
                                               max(0.0f, historySample.z));
                    }
                }

                // If reprojection found no valid history (skipped pixel or
                // out-of-bounds), use the sample as its own history so the
                // blend weight becomes irrelevant and no darkening occurs.
                if (historySample.x == 0.0f && historySample.y == 0.0f && historySample.z == 0.0f)
                    historySample = sample;

                m_pAccumulator[idx] = w * historySample + (1.0f - w) * sample;
            }
        }

        m_frameIndex++;
        m_spp = min(m_spp + 1, rendering::config::g_kMaxSpp);

        presentAccumulator();

        memcpy(m_pHistory, m_pAccumulator,
               static_cast<size_t>(width * height) * sizeof(float3));

        m_prevFrustum    = m_camera.buildFrustum();
        m_prevCamPos     = m_camera.m_camPos;
        m_prevCamTarget  = m_camera.m_camTarget;
        m_prevCamToWorld = m_camera.m_camToWorld;

        static float avg = 10.0f, alpha = 1.0f;
        avg = (1.0f - alpha) * avg + alpha * t.elapsed() * 1000.0f;
        if (alpha > 0.05f) alpha *= 0.5f;

        m_lastDiag.m_frameTimeMs = avg;
        m_lastDiag.m_fps         = avg > 0.0f ? 1000.0f / avg : 0.0f;
        m_lastDiag.m_mraysPerSec = static_cast<float>(width * height) / avg / 1000.0f;
        m_lastDiag.m_spp         = m_spp;
        m_lastDiag.m_cameraPos   = m_camera.m_camPos;
        m_lastDiag.m_cameraDir   = float3(m_camera.m_camToWorld.cell[2],
                                           m_camera.m_camToWorld.cell[6],
                                           m_camera.m_camToWorld.cell[10]);
    }

    void Renderer::resize(const int w, const int h)
    {
        if (w == m_renderWidth && h == m_renderHeight) return;
        shutdown();
        m_renderWidth  = w;
        m_renderHeight = h;
        initialize();
    }
}  // namespace rt::core