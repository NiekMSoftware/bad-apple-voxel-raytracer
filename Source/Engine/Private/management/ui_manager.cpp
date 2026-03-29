#include "rt/management/ui_manager.h"
#include "imgui.h"
#include "imgui_internal.h"   // DockBuilder API

namespace rt::management {

    UIManager::UIManager(LightManager&               lights,
                         rendering::MaterialManager& materials,
                         core::Camera&               camera,
                         core::SkyDome&              skyDome)
        : m_rendererPanel(camera, skyDome)
        , m_lightManager(lights)
        , m_materialManager(materials)
    {}

    // =========================================================================

    bool UIManager::render(const DiagnosticsData& data,
                           float&          exposure,
                           bool&           accumulate,
                           const uint32_t* pixels,
                           int&            renderW,
                           int&            renderH)
    {
        bool resetRequested = false;

        renderMenuBar(data, accumulate, resetRequested);
        renderDockSpace();

        if (m_bShowDiagnostics)
            renderDiagnostics(data);

        // Collect the resolution the user may have selected and the display
        // scale.  outRenderW/H start at the current values so they are
        // unchanged when the user has not touched the preset combo.
        int   newW  = renderW;
        int   newH  = renderH;
        float scale = 1.0f;

        bool changed = resetRequested;
        changed |= m_rendererPanel.render(data.m_spp, exposure, accumulate,
                                           newW, newH, scale);
        changed |= m_lightEditor.render(m_lightManager);
        changed |= m_materialEditor.render(m_materialManager);

        // The viewport always uses the *current* (pre-resize) pixel buffer and
        // dimensions so the frame displayed matches what was actually rendered.
        m_viewportPanel.render(pixels, renderW, renderH, scale);

        // Propagate any resolution change back to the caller.
        renderW = newW;
        renderH = newH;

        return changed;
    }

    // =========================================================================

    void UIManager::scanHdrFiles(const std::string& directory,
                                  const std::string& defaultFile)
    {
        m_rendererPanel.scanHdrFiles(directory, defaultFile);
    }

    // =========================================================================
    // Private helpers
    // =========================================================================

    void UIManager::renderDockSpace()
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();

        // BeginMainMenuBar height = FontSize + FramePadding.y * 2.
        // Reference: imgui.cpp, BeginMainMenuBar() window size calculation.
        const float menuH = ImGui::GetFrameHeight();

        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + menuH));
        ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - menuH));
        ImGui::SetNextWindowViewport(vp->ID);

        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDocking             |
            ImGuiWindowFlags_NoTitleBar            |
            ImGuiWindowFlags_NoCollapse            |
            ImGuiWindowFlags_NoResize              |
            ImGuiWindowFlags_NoMove                |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        ImGui::Begin("##DockSpace", nullptr, flags);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        m_dockspaceId = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(m_dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        // ---------------------------------------------------------------
        // Default layout - built once when no imgui.ini layout exists.
        // ---------------------------------------------------------------
        if (ImGui::DockBuilderGetNode(m_dockspaceId) == nullptr)
        {
            const ImVec2 size = ImVec2(vp->WorkSize.x, vp->WorkSize.y - menuH);

            ImGui::DockBuilderRemoveNode(m_dockspaceId);
            ImGui::DockBuilderAddNode(m_dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(m_dockspaceId, size);

            // Bottom strip
            ImGuiID idTop, idBottom;
            ImGui::DockBuilderSplitNode(m_dockspaceId, ImGuiDir_Down,
                                        0.22f, &idBottom, &idTop);

            // Left strip
            ImGuiID idLeft, idCentreRight;
            ImGui::DockBuilderSplitNode(idTop, ImGuiDir_Left,
                                        0.22f, &idLeft, &idCentreRight);

            // Right strip
            ImGuiID idCentre, idRight;
            ImGui::DockBuilderSplitNode(idCentreRight, ImGuiDir_Right,
                                        0.28f, &idRight, &idCentre);

            ImGui::DockBuilderDockWindow("Viewport",          idCentre);
            ImGui::DockBuilderDockWindow("Renderer Settings", idLeft);
            ImGui::DockBuilderDockWindow("Material Editor",   idRight);
            ImGui::DockBuilderDockWindow("Light Editor",      idRight);
            ImGui::DockBuilderDockWindow("Diagnostics",       idBottom);
            ImGui::DockBuilderDockWindow("Bad Apple",         idBottom);

            ImGui::DockBuilderFinish(m_dockspaceId);
        }

        ImGui::End();
    }

    // -------------------------------------------------------------------------

    void UIManager::renderMenuBar(const DiagnosticsData& data,
                                   bool& accumulate,
                                   bool& resetRequested)
    {
        if (!ImGui::BeginMainMenuBar()) return;

        // --- Windows menu ---
        if (ImGui::BeginMenu("Windows"))
        {
            if (ImGui::MenuItem("Viewport",          nullptr, m_viewportPanel.isVisible()))
                m_viewportPanel.toggle();
            if (ImGui::MenuItem("Diagnostics",       nullptr, m_bShowDiagnostics))
                m_bShowDiagnostics = !m_bShowDiagnostics;
            if (ImGui::MenuItem("Renderer Settings", nullptr, m_rendererPanel.isVisible()))
                m_rendererPanel.toggle();
            if (ImGui::MenuItem("Light Editor",      nullptr, m_lightEditor.isVisible()))
                m_lightEditor.toggle();
            if (ImGui::MenuItem("Material Editor",   nullptr, m_materialEditor.isVisible()))
                m_materialEditor.toggle();
            ImGui::EndMenu();
        }

        ImGui::Separator();

        // --- Reset button ---
        if (ImGui::Button(" Reset "))
            resetRequested = true;

        ImGui::Spacing();

        // --- Accumulate toggle button ---
        if (accumulate)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.15f, 1.0f));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));

        char accLabel[32];
        if (accumulate)
            snprintf(accLabel, sizeof(accLabel), " Accumulate  %d spp ", data.m_spp);
        else
            snprintf(accLabel, sizeof(accLabel), " Accumulate  off ");

        if (ImGui::Button(accLabel))
        {
            accumulate     = !accumulate;
            resetRequested = true;
        }
        ImGui::PopStyleColor();

        // --- Right-aligned stats ---
        char stats[80];
        snprintf(stats, sizeof(stats), "%.1f FPS  |  %.2f ms  |  %.1f MRays/s ",
                 data.m_fps, data.m_frameTimeMs, data.m_mraysPerSec);
        const float statsW = ImGui::CalcTextSize(stats).x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - statsW);
        ImGui::TextDisabled("%s", stats);

        ImGui::EndMainMenuBar();
    }

    // -------------------------------------------------------------------------

    void UIManager::renderDiagnostics(const DiagnosticsData& data)
    {
        ImGui::SetNextWindowSize(ImVec2(290.0f, 230.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.92f);

        if (!ImGui::Begin("Diagnostics", &m_bShowDiagnostics))
        { ImGui::End(); return; }

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Performance");
        ImGui::Separator();
        ImGui::Text("Frame time  : %.2f ms",  data.m_frameTimeMs);
        ImGui::Text("FPS         : %.1f",     data.m_fps);
        ImGui::Text("MRays/s     : %.2f",     data.m_mraysPerSec);
        ImGui::Text("SPP         : %d",       data.m_spp);

        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Camera");
        ImGui::Separator();
        ImGui::Text("Pos : %.2f  %.2f  %.2f",
            data.m_cameraPos.x, data.m_cameraPos.y, data.m_cameraPos.z);
        ImGui::Text("Dir : %.2f  %.2f  %.2f",
            data.m_cameraDir.x, data.m_cameraDir.y, data.m_cameraDir.z);

        ImGui::Spacing();

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Scene");
        ImGui::Separator();
        ImGui::Text("Lights      : %u total", m_lightManager.totalCount());
        ImGui::Text("  Dir %u  |  Pt %u  |  Spot %u",
            m_lightManager.directionalCount(),
            m_lightManager.pointCount(),
            m_lightManager.spotCount());
        ImGui::Text("Materials   : %u", m_materialManager.getMaterialCount());

        ImGui::End();
    }

}  // namespace rt::management