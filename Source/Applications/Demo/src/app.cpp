#include "app.h"
#include "scene_builder.h"

namespace demo {

    DemoApplication::DemoApplication()
        : m_uiManager(
              m_renderer.getLightManager(),
              m_renderer.getMaterialManager(),
              m_renderer.getCamera(),
              m_renderer.getSkyDome())
    {}

    void DemoApplication::Init()
    {
        m_renderer.initialize();
        m_renderer.setAccumulation(false);
        m_uiManager.scanHdrFiles("assets/textures", "");
        buildScene();

        m_badApple.load("assets/bad_apple.bin",
                        "assets/bad_apple.mp3");
        m_badApple.m_materialId = 0;

        m_splineEditor.tryLoad(m_posSpline, m_targetSpline,
                               m_renderer.getCamera());
    }

    void DemoApplication::buildScene()
    {
        buildTheatreFloor(m_renderer.getScene());
        buildAudienceSpheres(m_renderer.getScene().m_analyticScene);
        buildTheatreLights(m_renderer.getLightManager());
        m_renderer.getScene().m_analyticScene.buildBvh();

        auto& cam       = m_renderer.getCamera();
        cam.m_camPos    = float3(0.50f, 0.55f, -0.20f);
        cam.m_camTarget = float3(0.50f, 0.50f,  0.50f);
        cam.updateCameraToWorld();
    }

    void DemoApplication::Shutdown()
    {
        m_renderer.shutdown();
    }

    void DemoApplication::Tick(const float deltaTime)
    {
        auto& cam = m_renderer.getCamera();

        // Tab: toggle free-cam
        static bool s_tabWasDown = false;
        const bool  tabIsDown    = IsKeyDown(GLFW_KEY_TAB);
        if (tabIsDown && !s_tabWasDown)
            m_bCameraFree = !m_bCameraFree;
        s_tabWasDown = tabIsDown;

        // Bad Apple update + restart detection
        if (m_renderer.getAccumulation() && m_badApple.update(m_renderer.getScene(), deltaTime))
            m_renderer.resetAccumulator();
        else
            m_badApple.update(m_renderer.getScene(), deltaTime);

        // consumeRestart is still needed so the camera snaps back correctly
        // when the video loops -- getPlaybackTime() returns 0 after a loop
        // but we want to guarantee the spline also resets instantly.
        if (m_badApple.consumeRestart())
            m_splineTime = 0.0f;

        // ---------------------------------------------------------------------
        // Camera update
        //
        // m_splineTime is driven directly by the bad apple playback cursor so
        // it is always in sync with the video and audio, including scrubbing.
        // ---------------------------------------------------------------------
        if (m_bCameraFree)
        {
            cam.handleInput(deltaTime);
        }
        else if (m_posSpline.keyframeCount() >= 2)
        {
            // Sync spline time to the bad apple playback position.
            // Clamp to endTime so the camera holds at the last keyframe
            // once the spline is shorter than the video.
            const float endT = m_posSpline.endTime();
            m_splineTime = m_badApple.getPlaybackTime();
            if (m_splineTime > endT)
                m_splineTime = endT;

            cam.m_camPos    = m_posSpline.evaluate(m_splineTime);
            cam.m_camTarget = m_targetSpline.evaluate(m_splineTime);

            cam.m_lensRadius    = 0.0f;
            cam.m_focalDistance = 2.0f;
            cam.updateCameraToWorld();
        }

        // --- Animate the point light (index 0 in the point light list) ---
        {
            m_pointLightTime += deltaTime;

            auto& pointLights = m_renderer.getLightManager().getPointLights();
            if (!pointLights.empty())
            {
                // Lissajous orbit — three sine waves at different frequencies
                // keep the light within the theatre floor bounds [0.15, 0.85]
                const float cx = 0.50f, cy = 0.45f, cz = 0.35f;  // orbit centre
                const float rx = 0.30f, ry = 0.25f, rz = 0.10f;  // orbit radii

                const float px = cx + rx * sinf(m_pointLightTime * 0.4f);
                const float py = cy + ry * sinf(m_pointLightTime * 0.6f);
                const float pz = cz + rz * sinf(m_pointLightTime * 0.3f);
                pointLights[0].setPosition(float3(px, py, pz));

                // HSV to RGB with S=1, V=1 — full spectrum cycle
                // Reference: Foley et al., "Computer Graphics: Principles and
                // Practice", 2nd ed., Section 13.3.4 (HSV to RGB conversion)
                const float hue = fmodf(m_pointLightTime * 0.15f, 1.0f);  // 0..1
                const float h6  = hue * 6.0f;
                const int   hi  = static_cast<int>(h6);
                const float f   = h6 - static_cast<float>(hi);

                float r = 0.0f, g = 0.0f, b = 0.0f;
                switch (hi % 6)
                {
                    case 0: r = 1.0f; g = f;        b = 0.0f;     break;
                    case 1: r = 1.0f - f; g = 1.0f; b = 0.0f;     break;
                    case 2: r = 0.0f; g = 1.0f;     b = f;         break;
                    case 3: r = 0.0f; g = 1.0f - f; b = 1.0f;     break;
                    case 4: r = f;    g = 0.0f;     b = 1.0f;     break;
                    case 5: r = 1.0f; g = 0.0f;     b = 1.0f - f; break;
                    default: ;
                }
                pointLights[0].setColor(float3(r, g, b));

                // Breathing intensity
                // sin() oscillates [-1, 1], remap to [minI, maxI].
                // Period ~4 seconds (2*PI / 1.5 ≈ 4.19s) feels like
                // a calm, natural breath cycle.
                constexpr float minI = 0.1f;
                constexpr float maxI = 0.6f;
                const float breath = sinf(m_pointLightTime * 1.5f);
                const float intensity = minI + (maxI - minI) * (breath * 0.5f + 0.5f);
                pointLights[0].setIntensity(intensity);
            }
        }

        m_renderer.renderFrame();
    }

    void DemoApplication::UI()
    {
        float exposure   = m_renderer.getExposure();
        bool  accumulate = m_renderer.getAccumulation();
        int   renderW    = m_renderer.getRenderWidth();
        int   renderH    = m_renderer.getRenderHeight();

        if (m_uiManager.render(
                m_renderer.getDiagnostics(),
                exposure,
                accumulate,
                m_renderer.getPixelBuffer(),
                renderW,
                renderH))
            m_renderer.resetAccumulator();

        m_renderer.setExposure(exposure);
        m_renderer.setAccumulation(accumulate);

        if (renderW != m_renderer.getRenderWidth() ||
            renderH != m_renderer.getRenderHeight())
            m_renderer.resize(renderW, renderH);

        m_badApple.renderUI(m_renderer.getScene());

        m_splineEditor.render(m_posSpline, m_targetSpline,
                              m_renderer.getCamera(), m_splineTime);

        if (m_bCameraFree)
        {
            ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f));
            ImGui::SetNextWindowSize(ImVec2(300.0f, 48.0f));
            ImGui::SetNextWindowBgAlpha(0.75f);
            if (ImGui::Begin("##freecam", nullptr))
            {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                    "FREE CAM  (Tab to return to spline)");
                ImGui::Text("WASD move  IJKL look  Space/Ctrl up/dn");
            }
            ImGui::End();
        }
    }

}  // namespace demo