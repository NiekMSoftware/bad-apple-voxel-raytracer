#pragma once

#include "rt/core/camera.h"
#include "rt/core/ray.h"
#include "rt/core/sky_dome.h"
#include "rt/core/blue_noise.h"
#include "rt/scene/scene.h"
#include "rt/rendering/material_manager.h"
#include "rt/management/light_manager.h"
#include "rt/management/ui_manager.h"
#include "rt/rendering/config.h"

namespace rt::core {

    // =========================================================================
    // Renderer
    //
    // =========================================================================
    class Renderer
    {
    public:
        Renderer();
        ~Renderer();

        Renderer(const Renderer&)            = delete;
        Renderer& operator=(const Renderer&) = delete;

        // --- Lifecycle ---
        void initialize();
        void shutdown();

        // Trace one sample per pixel at the internal render resolution.
        // Result is available via getPixelBuffer() immediately after.
        void renderFrame();

        // --- Render resolution ---
        void resize(int w, int h);
        [[nodiscard]] int getRenderWidth()  const { return m_renderWidth;  }
        [[nodiscard]] int getRenderHeight() const { return m_renderHeight; }

        // Reallocates per-pixel buffers, resets accumulator, and updates
        // the camera so ray directions match the new resolution.
        void setRenderResolution(int w, int h);

        // Latest tonemapped output at getRenderWidth() x getRenderHeight().
        [[nodiscard]] const uint32_t* getPixelBuffer() const { return m_pPixelBuffer; }

        // --- Accessors ---
        Camera&                     getCamera()          { return m_camera;          }
        const Camera&               getCamera()    const { return m_camera;          }
        scene::Scene&               getScene()           { return m_scene;           }
        SkyDome&                    getSkyDome()         { return m_skyDome;         }
        management::LightManager&   getLightManager()    { return m_lightManager;    }
        rendering::MaterialManager& getMaterialManager() { return m_materialManager; }

        // --- Exposure ---
        [[nodiscard]] float getExposure() const  { return m_exposure; }
        void                setExposure(float e) { m_exposure = e;    }

        // --- Accumulation ---
        [[nodiscard]] bool getAccumulation() const { return m_bAccumulate; }
        void               setAccumulation(bool b) { m_bAccumulate = b;    }

        // --- SPP ---
        [[nodiscard]] int getSpp() const { return m_spp; }

        void resetAccumulator();

        [[nodiscard]] const management::DiagnosticsData& getDiagnostics() const { return m_lastDiag; }

    private:
        void allocateBuffers();
        void freeBuffers();

        [[nodiscard]] float3 trace(Ray& ray, float maxRayDist = 1e34f,
                                   float* primaryDepthOut = nullptr) const;
        [[nodiscard]] float3 sampleSky(const Ray& ray, int bounce) const;

        void accumulatePixel(int idx, const float3& newSample) const;
        void presentAccumulator() const;

        // --- Frame state ---
        int   m_spp         = 0;
        uint  m_frameIndex  = 0;
        float m_exposure    = 1.0f;
        bool  m_bAccumulate = true;

        // --- Render resolution ---
        int m_renderWidth  = 800;
        int m_renderHeight = 600;

        // --- Per-pixel buffers ---
        uint32_t* m_pPixelBuffer  = nullptr;
        int*      m_pSampleCount  = nullptr;
        float3*   m_pAccumulator  = nullptr;
        float3*   m_pNewSamples   = nullptr;
        float3*   m_pRayDir       = nullptr;
        float3*   m_pHistory      = nullptr;
        float*    m_pHistoryDepth = nullptr;
        float*    m_pDepth        = nullptr;

        // --- Reprojection state ---
        float3  m_prevCamPos     = {};
        float3  m_prevCamTarget  = {};
        mat4    m_prevCamToWorld = {};
        Frustum m_prevFrustum    = {};
        bool    m_bPrevCameraMoved = false;

        // --- Scene / rendering objects ---
        scene::Scene                m_scene;
        Camera                      m_camera;
        SkyDome                     m_skyDome;
        management::LightManager    m_lightManager;
        rendering::MaterialManager  m_materialManager;
        management::DiagnosticsData m_lastDiag;
        BlueNoise                   m_noise;

        rendering::ShadingServices  m_shadingServices;
    };

}  // namespace rt::core