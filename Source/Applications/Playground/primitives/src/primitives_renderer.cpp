#include "primitives_renderer.h"

#include <filesystem>

#include "rt/rendering/primitives/hit_info.h"
#include "rt/rendering/primitives/sphere.h"
#include "rt/rendering/bit_packer.h"
#include "rt/brdf/brdf_fresnel.h"
#include "tmpl8/tmpl8math.h"

// ============================================================================
// Scene construction helpers (file-local)
// ============================================================================

namespace {

#define SINGLE_SPHERE 0

    constexpr uint g_kMaxBalls = 1'000;

    // ------------------------------------------------------------------------
    void buildTileFloor(rt::scene::Scene& scene)
    {
        constexpr uint xMin = 100, xMax = 400;
        constexpr uint zMin = 100, zMax = 400;

        for (uint x = xMin; x < xMax; x++)
            for (uint z = zMin; z < zMax; z++)
            {
                constexpr uint tile      = 8;
                constexpr uint y         = 0;
                const bool     lightTile = ((x / tile) + (z / tile)) % 2 == 0;
                const uint     v         = lightTile
                    ? rt::rendering::packVoxel(0, 220, 220, 220)   // light grey
                    : rt::rendering::packVoxel(1,  60,  60,  60);  // dark grey
                scene.set(x, y, z, v);
            }
    }

    // ------------------------------------------------------------------------
    void spawnSpheres(rt::primitives::AnalyticScene&        analyticScene,
                      const rt::rendering::MaterialManager& matMgr)
    {
#if SINGLE_SPHERE
        (void)matMgr;

        for (uint i = 0; i < 3; i++)
        {
            rt::primitives::Sphere s;
            s.m_radius   = 0.03f;
            s.m_center.x = 0.35f + static_cast<float>(i) * 0.1f;
            s.m_center.z = 0.5f;
            s.m_center.y = s.m_radius + 1.0f / WORLDSIZE;
            s.m_matIndex = 5;  // Chrome
            analyticScene.addSphere(s);
        }
#else
        const uint matCount = matMgr.getMaterialCount();

        uint seed = 0xDEADBEEF;
        auto rnd = [&](const float lo, const float hi) -> float {
            seed = seed * 1664525u + 1013904223u;
            const float t = (seed >> 8) / static_cast<float>(1 << 24);
            return lo + t * (hi - lo);
        };

        for (uint i = 0; i < g_kMaxBalls; i++)
        {
            rt::primitives::Sphere s;
            s.m_radius   = rnd(0.003f, 0.012f);
            s.m_center.x = rnd(0.22f, 0.76f);
            s.m_center.z = rnd(0.22f, 0.76f);
            s.m_center.y = s.m_radius + 1.0f / WORLDSIZE;

            const float roll = rnd(0.0f, 1.0f);
            if      (roll < 0.01f) s.m_matIndex = 2;   // Glass   - 1%
            else if (roll < 0.015f) s.m_matIndex = 10;  // Water   - 0.5%
            else if (roll < 0.02f) s.m_matIndex = 11;  // Diamond - 0.5%
            else                   s.m_matIndex = i % matCount;

            analyticScene.addSphere(s);
        }
#endif

        analyticScene.buildBvh();
    }

    // ------------------------------------------------------------------------
    void registerDemoMaterials(rt::rendering::MaterialManager& matMgr)
    {
        auto addDiffuse = [&](const float r, const float g, const float b) {
            rt::rendering::Material m = rt::rendering::MaterialManager::createDefaultMaterial();
            m.m_baseColor = float3(r, g, b);
            m.m_metallic  = 0.0f;
            m.m_roughness = 0.6f;
            matMgr.registerMaterial(m);
        };

        auto addMetal = [&](const float r, const float g, const float b, const float roughness) {
            rt::rendering::Material m = rt::rendering::MaterialManager::createDefaultMaterial();
            m.m_baseColor = float3(r, g, b);
            m.m_metallic  = 1.0f;
            m.m_roughness = roughness;
            matMgr.registerMaterial(m);
        };

        addDiffuse(1.0f, 0.2f, 0.2f);             // red
        addDiffuse(0.2f, 0.8f, 0.2f);             // green
        addDiffuse(0.2f, 0.4f, 1.0f);             // blue
        addDiffuse(1.0f, 0.8f, 0.1f);             // yellow
        addDiffuse(1.0f, 0.4f, 0.0f);             // orange
        addDiffuse(0.8f, 0.2f, 0.8f);             // purple
        addDiffuse(0.0f, 0.9f, 0.9f);             // cyan
        addDiffuse(1.0f, 0.6f, 0.7f);             // pink
        addDiffuse(0.9f, 0.9f, 0.9f);             // white
        addMetal(1.0f,  0.76f, 0.33f, 0.10f);    // gold
        addMetal(0.95f, 0.93f, 0.88f, 0.05f);    // chrome
        addMetal(0.95f, 0.64f, 0.54f, 0.20f);    // copper
    }

}  // anonymous namespace

// ============================================================================

namespace Tmpl8::primitives {

    // =========================================================================
    // Constructor
    // =========================================================================

    PrimitivesRenderer::PrimitivesRenderer()
        : m_uiManager(m_lightManager, m_materialManager)
    {}

    // =========================================================================
    // Init / Shutdown
    // =========================================================================

    void PrimitivesRenderer::Init()
    {
        m_materialManager.initialize();
        m_skyDome.load("assets/textures/minedump_flats_4k.hdr");
        scanHdrFiles();

        buildScene();

        constexpr int pixelCount = SCRWIDTH * SCRHEIGHT;
        m_pAccumulator = new float3[pixelCount];
        m_pPrevFrame   = new float3[pixelCount];
        resetAccumulator();
    }

    void PrimitivesRenderer::buildScene()
    {
        // buildTileFloor(m_scene);
        registerDemoMaterials(m_materialManager);
        spawnSpheres(m_scene.m_analyticScene, m_materialManager);
    }

    void PrimitivesRenderer::scanHdrFiles()
    {
        m_hdrFiles.clear();

        const std::filesystem::path searchDir = "assets/textures";

        if (!std::filesystem::exists(searchDir)) return;

        for (const auto& entry : std::filesystem::directory_iterator(searchDir))
        {
            if (!entry.is_regular_file()) continue;

            const auto& path = entry.path();
            if (path.extension() == ".hdr" || path.extension() == ".HDR")
                m_hdrFiles.push_back(path.string());
        }

        std::sort(m_hdrFiles.begin(), m_hdrFiles.end());

        for (int i = 0; i < static_cast<int>(m_hdrFiles.size()); i++)
        {
            if (m_hdrFiles[i] == "assets/textures/minedump_flats_4k.hdr")
            {
                m_selectedHdr = i;
                break;
            }
        }
    }

    void PrimitivesRenderer::Shutdown()
    {
        delete[] m_pAccumulator; m_pAccumulator = nullptr;
        delete[] m_pPrevFrame;   m_pPrevFrame   = nullptr;
    }

    // =========================================================================
    // Static helpers
    // =========================================================================

    bool PrimitivesRenderer::survivesRussianRoulette(float3& throughput, const int bounce)
    {
        if (bounce < config::g_kRussianRouletteMinBounce) return true;

        const float pSurvive = min(
            max(max(throughput.x, throughput.y), throughput.z), 1.0f);

        if (RandomFloat() > pSurvive)
            return false;

        throughput *= 1.0f / pSurvive;
        return true;
    }

    float3 PrimitivesRenderer::beerLambertAbsorption(const float3& baseColor,
                                                     const float rayT,
                                                     const float primRadius)
    {
        return beerLambertSSE(baseColor, rayT, primRadius);  // SIMD
    }

    // =========================================================================
    // IBL helper
    // =========================================================================

    float3 PrimitivesRenderer::sampleIbl(const float3& biasedI, const float3& N) const
    {
        if (!m_skyDome.isLoaded()) return float3(0.0f);

        const float3 iblDir = rt::core::SkyDome::cosineSampleHemisphere(
            N, RandomFloat(), RandomFloat());

        rt::core::Ray iblRay(biasedI, iblDir, 1.74f);
        if (m_scene.isOccluded(iblRay)) return {0};

        return clampFireflySSE(m_skyDome.sample(iblDir) * config::g_kIblWeight);  // SIMD
    }

    // =========================================================================
    // Per-hit shading methods
    // =========================================================================

    BounceResult PrimitivesRenderer::shadeDielectric(rt::core::Ray& ray,
                                                     const float3&  I,
                                                     const float3&  N,
                                                     const float3&  V,
                                                     const rt::rendering::Material& mat,
                                                     const int   bounce,
                                                     const float maxRayDist,
                                                     float3& throughput) const
    {
        BounceResult result{};
        result.m_bContinue = true;

        const bool   entering = dot(N, V) > 0.0f;
        const float3 faceN    = entering ? N : -N;
        const float3 biasedI  = I + faceN * EPSILON;

        if (bounce == 0 && mat.m_transparency < 1.0f)
        {
            const float opacity = 1.0f - mat.m_transparency;
            result.m_color = m_lightManager.calculateLighting(
                biasedI, faceN, V, mat, m_scene, m_materialManager)
                * opacity * opacity;
        }

        const float n1   = entering ? 1.0f : mat.m_indexOfRefraction;
        const float n2   = entering ? mat.m_indexOfRefraction : 1.0f;
        const float eta  = n1 / n2;
        const float cosI = dot(faceN, V);
        const float fresnel = rt::brdf::fresnelSchlickScalar(cosI, n1, n2);

        const float sin2T = eta * eta * (1.0f - cosI * cosI);
        const bool  tir   = (sin2T >= 1.0f);

        if (tir || RandomFloat() < fresnel)
        {
            const float3 reflDir = ray.m_d - 2.0f * dot(ray.m_d, faceN) * faceN;
            ray = rt::core::Ray(biasedI, reflDir, maxRayDist);
        }
        else
        {
            const float oldT          = ray.m_t;
            const float oldPrimRadius = ray.m_primRadius;

            const float  cosT    = sqrtf(1.0f - sin2T);
            const float3 refrDir = eta * ray.m_d + (eta * cosI - cosT) * faceN;
            ray = rt::core::Ray(I - faceN * EPSILON, refrDir, maxRayDist);

            if (!entering)
                throughput *= beerLambertSSE(mat.m_baseColor, oldT, oldPrimRadius);  // SIMD
        }

        return result;
    }

    // ------------------------------------------------------------------------

    BounceResult PrimitivesRenderer::shadeOpaqueSphere(rt::core::Ray& ray,
                                                       const float3&  I,
                                                       const float3&  N,
                                                       const float3&  V,
                                                       const rt::rendering::Material& mat,
                                                       const int   bounce,
                                                       const float maxRayDist) const
    {
        BounceResult result{};

        const bool   entering = dot(N, V) > 0.0f;
        const float3 faceN    = entering ? N : -N;
        const float3 biasedI  = I + faceN * EPSILON;

        result.m_color = (bounce > 0)
            ? m_lightManager.calculateLighting(biasedI, N, V, mat, m_scene)
            : m_lightManager.calculateLighting(biasedI, N, V, mat, m_scene, m_materialManager);

        if (RandomFloat() < config::g_kIblRussianRouletteProbability)
            result.m_color += mat.m_baseColor
                * sampleIbl(biasedI, N) * (1.0f / config::g_kIblRussianRouletteProbability);

        if (mat.m_roughness < config::g_kReflectionRoughnessThreshold)
        {
            const float3 f0 = rt::brdf::calculateF0(
                mat.m_baseColor, mat.m_metallic, mat.m_indexOfRefraction);
            const float  cosTheta = max(dot(N, V), 0.0f);
            const float3 f = rt::brdf::fresnelSchlick(cosTheta, f0);

            result.m_throughputScale = f;
            result.m_bContinue       = true;

            const float3 reflDir = ray.m_d - 2.0f * dot(ray.m_d, N) * N;
            ray = rt::core::Ray(biasedI, reflDir, maxRayDist);
        }

        return result;
    }

    // ------------------------------------------------------------------------

    BounceResult PrimitivesRenderer::shadeVoxel(const rt::core::Ray& ray,
                                                const int bounce) const
    {
        BounceResult result{};

        result.m_color = (bounce > 0)
            ? m_lightManager.calculateLighting(ray, m_scene)
            : m_lightManager.calculateLighting(ray, m_scene, m_materialManager);

        if (RandomFloat() < config::g_kIblRussianRouletteProbability)
        {
            const float3 N       = ray.getNormal();
            const float3 I       = ray.intersectionPoint();
            const float3 biasedI = I + N * EPSILON;

            result.m_color += ray.getAlbedo()
                * sampleIbl(biasedI, N) * (1.0f / config::g_kIblRussianRouletteProbability);
        }

        return result;
    }

    // =========================================================================
    // Trace — iterative path tracer
    // =========================================================================

    float3 PrimitivesRenderer::trace(rt::core::Ray& ray, const float maxRayDist) const
    {
        float3 color{0};
        float3 throughput{1.0f};

        for (int bounce = 0; bounce < config::g_kMaxDepth; bounce++)
        {
            if (!survivesRussianRoulette(throughput, bounce))
            {
                if (m_skyDome.isLoaded()) {
                    float3 skyColor = m_skyDome.sample(ray.m_d);
                    if (bounce > 0)
                        skyColor = clampFireflySSE(skyColor);  // SIMD
                    color += throughput * skyColor;
                }
                break;
            }

            ray.m_t = fminf(ray.m_t, maxRayDist);
            m_scene.findNearest(ray);

            if (ray.m_voxel == 0)
            {
                if (m_skyDome.isLoaded())
                {
                    float3 skyColor = m_skyDome.sample(ray.m_d);
                    if (bounce > 0)
                        skyColor = clampFireflySSE(skyColor);  // SIMD
                    color += throughput * skyColor;
                }
                break;
            }

            BounceResult result{};

            if (ray.m_voxel == 0x40000000u)
            {
                const rt::rendering::Material& mat =
                    m_materialManager.getMaterial(ray.m_primMatIndex);
                const float3 I = ray.intersectionPoint();
                const float3 N = ray.m_primNormal;
                const float3 V = normalize(-ray.m_d);

                result = (mat.m_transparency > 0.0f)
                    ? shadeDielectric(ray, I, N, V, mat, bounce, maxRayDist, throughput)
                    : shadeOpaqueSphere(ray, I, N, V, mat, bounce, maxRayDist);
            }
            else
            {
                result = shadeVoxel(ray, bounce);
            }

            color += throughput * result.m_color;

            if (!result.m_bContinue) break;
            throughput *= result.m_throughputScale;
        }

        return color;
    }

    // =========================================================================
    // Accumulator helpers
    // =========================================================================

    void PrimitivesRenderer::resetAccumulator()
    {
        constexpr int pixelCount = SCRWIDTH * SCRHEIGHT;
        memset(m_pAccumulator, 0, static_cast<size_t>(pixelCount) * sizeof(float3));
        memset(m_pPrevFrame,   0, static_cast<size_t>(pixelCount) * sizeof(float3));
        m_spp        = 0;
        m_frameIndex = 0;
    }

    void PrimitivesRenderer::accumulatePixel(const int idx, const float3& newSample) const
    {
        const float wOld    = static_cast<float>(m_spp) / static_cast<float>(m_spp + 1);
        const float wNew    = 1.0f - wOld;
        m_pAccumulator[idx] = newSample * wNew + m_pAccumulator[idx] * wOld;
    }

    void PrimitivesRenderer::presentAccumulator() const
    {
        presentAccumulatorAVX(                  // SIMD: Reinhard + gamma for all pixels
            m_pAccumulator,
            screen->pixels,
            SCRWIDTH * SCRHEIGHT,
            m_exposure);
    }

    // =========================================================================
    // Tick
    // =========================================================================

    void PrimitivesRenderer::Tick(const float deltaTime)
    {
        const Timer t;

        m_spp++;

#pragma omp parallel for schedule(dynamic)
        for (int y = 0; y < SCRHEIGHT; y++)
        {
            for (int x = 0; x < SCRWIDTH; x++)
            {
                const int  idx            = x + y * SCRWIDTH;
                const bool traceThisPixel = ((x + y) & 1) == (static_cast<int>(m_frameIndex) & 1);

                float3 sample{};
                if (traceThisPixel)
                {
                    rt::core::Ray r = m_camera.getPrimaryRay(
                        static_cast<float>(x) + RandomFloat(),
                        static_cast<float>(y) + RandomFloat());
                    sample = trace(r);
                }
                else
                {
                    sample = m_pPrevFrame[idx];
                }

                accumulatePixel(idx, sample);
                m_pPrevFrame[idx] = sample;
            }
        }

        m_frameIndex++;
        m_spp = min(m_spp, config::g_kMaxSpp);

        presentAccumulator();

        static float avg = 10.0f, alpha = 1.0f;
        avg = (1.0f - alpha) * avg + alpha * t.elapsed() * 1000.0f;
        if (alpha > 0.05f) alpha *= 0.5f;

        m_lastDiag.m_frameTimeMs = avg;
        m_lastDiag.m_fps         = avg > 0.0f ? 1000.0f / avg : 0.0f;
        m_lastDiag.m_mraysPerSec = static_cast<float>(SCRWIDTH * SCRHEIGHT) / avg / 1000.0f;
        m_lastDiag.m_cameraPos   = m_camera.m_camPos;
        m_lastDiag.m_cameraDir = float3(
            m_camera.m_camToWorld.cell[2],
            m_camera.m_camToWorld.cell[6],
            m_camera.m_camToWorld.cell[10]);

        if (m_camera.handleInput(deltaTime)) resetAccumulator();
    }

    // =========================================================================
    // UI
    // =========================================================================

    void PrimitivesRenderer::UI()
    {
        if (m_uiManager.render(m_lastDiag)) resetAccumulator();

        ImGui::Text("Samples: %d", m_spp);

        bool changed = false;
        changed |= ImGui::DragFloat("FOV", &m_camera.m_fovDegrees, 0.01f, 1.0f, 112.0f);

        ImGui::Separator();
        ImGui::Text("Skydome");
        if (!m_hdrFiles.empty())
        {
            auto getFilename = [](const std::string& path) -> const char* {
                const auto pos = path.find_last_of("/\\");
                return pos == std::string::npos ? path.c_str() : path.c_str() + pos + 1;
            };

            if (ImGui::BeginCombo("HDR", getFilename(m_hdrFiles[m_selectedHdr])))
            {
                for (int i = 0; i < static_cast<int>(m_hdrFiles.size()); i++)
                {
                    const bool selected = (m_selectedHdr == i);
                    if (ImGui::Selectable(getFilename(m_hdrFiles[i]), selected))
                    {
                        m_selectedHdr = i;
                        m_skyDome.load(m_hdrFiles[i]);
                        changed = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            ImGui::TextDisabled("No .hdr files found in assets/textures/");
        }

        ImGui::Separator();
        ImGui::Text("Tone Mapping");
        ImGui::DragFloat("Exposure", &m_exposure, 0.01f, 0.0f, 10.0f, "%.2f");

        if (changed) resetAccumulator();
    }

}  // namespace Tmpl8::primitives