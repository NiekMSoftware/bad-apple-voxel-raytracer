#pragma once

#include "rt/core/renderer.h"
#include "rt/management/ui_manager.h"
#include "tmpl8/app.h"
#include "bad_apple_player.h"
#include "spline.h"
#include "spline_editor.h"

namespace demo {

    class DemoApplication : public Tmpl8::Application
    {
    public:
        DemoApplication();

        void Init()                override;
        void Tick(float deltaTime) override;
        void UI()                  override;
        void Shutdown()            override;

    private:
        void buildScene();

        rt::core::Renderer        m_renderer;
        rt::management::UIManager m_uiManager;

        Spline<float3> m_posSpline;
        Spline<float3> m_targetSpline;
        float          m_splineTime = 0.0f;
        float          m_pointLightTime = 0.0f;

        BadApplePlayer m_badApple;
        SplineEditor   m_splineEditor;

        // When true the spline is bypassed and Camera::handleInput() drives
        // the camera directly.  Toggled with Tab in Tick().
        bool m_bCameraFree = false;
    };

}  // namespace demo