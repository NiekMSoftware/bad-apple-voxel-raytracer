#pragma once

#include "rt/management/light_editor.h"
#include "rt/management/renderer_panel.h"
#include "rt/management/viewport_panel.h"
#include "rt/rendering/material_editor.h"
#include "rt/management/light_manager.h"
#include "rt/rendering/material_manager.h"

#include <cstdint>
#include <string>

namespace rt::core {
    class Camera;
    class SkyDome;
}

namespace rt::management {

    // =========================================================================
    // DiagnosticsData
    // =========================================================================
    struct DiagnosticsData
    {
        float  m_frameTimeMs = 0.0f;
        float  m_fps         = 0.0f;
        float  m_mraysPerSec = 0.0f;
        int    m_spp         = 0;
        float3 m_cameraPos   = float3(0);
        float3 m_cameraDir   = float3(0, 0, 1);
    };

    // =========================================================================
    // UIManager
    //
    // Single entry point for all per-frame ImGui rendering.
    //
    // render() returns true when the accumulator should be reset.
    // =========================================================================
    class UIManager
    {
    public:
        UIManager(LightManager&               lights,
                  rendering::MaterialManager& materials,
                  core::Camera&               camera,
                  core::SkyDome&              skyDome);
        ~UIManager() = default;

        // Call once per frame between ImGui::NewFrame() and ImGui::Render().
        //   data       : read-only diagnostics
        //   exposure   : in/out - caller writes back to renderer
        //   accumulate : in/out - caller writes back to renderer
        //   pixels     : renderer pixel buffer (for the viewport)
        //   renderW    : in/out - current width;  updated when user picks a preset
        //   renderH    : in/out - current height; updated when user picks a preset
        // Returns true when the accumulator should be reset.
        bool render(const DiagnosticsData& data,
                    float&          exposure,
                    bool&           accumulate,
                    const uint32_t* pixels,
                    int&            renderW,
                    int&            renderH);

        // Delegates to RendererPanel::scanHdrFiles().
        void scanHdrFiles(const std::string& directory   = "assets/textures",
                          const std::string& defaultFile = "");

        LightEditor&               getLightEditor()    { return m_lightEditor;    }
        rendering::MaterialEditor& getMaterialEditor() { return m_materialEditor; }
        RendererPanel&             getRendererPanel()  { return m_rendererPanel;  }
        ViewportPanel&             getViewportPanel()  { return m_viewportPanel;  }

    private:
        LightEditor               m_lightEditor;
        rendering::MaterialEditor m_materialEditor;
        RendererPanel             m_rendererPanel;
        ViewportPanel             m_viewportPanel;

        LightManager&               m_lightManager;
        rendering::MaterialManager& m_materialManager;

        bool      m_bShowDiagnostics = true;
        ImGuiID   m_dockspaceId      = 0;

        void renderDockSpace();
        void renderMenuBar(const DiagnosticsData& data,
                           bool& accumulate,
                           bool& resetRequested);
        void renderDiagnostics(const DiagnosticsData& data);
    };

}  // namespace rt::management