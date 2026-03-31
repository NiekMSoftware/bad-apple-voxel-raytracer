#include "rt/management/renderer_panel.h"
#include "rt/core/camera.h"
#include "rt/core/sky_dome.h"
#include "imgui.h"

#include <algorithm>
#include <filesystem>

namespace rt::management {

    RendererPanel::RendererPanel(core::Camera& camera, core::SkyDome& skyDome)
        : m_camera(camera), m_skyDome(skyDome)
    {}

    // -------------------------------------------------------------------------

    bool RendererPanel::render(const int spp,
                                float& exposure,
                                bool&  accumulate,
                                int&   outRenderW,
                                int&   outRenderH,
                                float& outScale)
    {
        if (!m_bVisible) return false;

        ImGui::SetNextWindowSize(ImVec2(300, 420), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Renderer Settings", &m_bVisible)) { ImGui::End(); return false; }

        bool changed = false;

        if (ImGui::CollapsingHeader("Resolution", ImGuiTreeNodeFlags_DefaultOpen))
            changed |= renderResolutionSection(outRenderW, outRenderH);

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Accumulation", ImGuiTreeNodeFlags_DefaultOpen))
            changed |= renderAccumulationSection(spp, accumulate);

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            changed |= renderCameraSection();
            changed |= renderDofSection();
        }

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Sky Dome", ImGuiTreeNodeFlags_DefaultOpen))
            changed |= renderSkySection();

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Tone Mapping", ImGuiTreeNodeFlags_DefaultOpen))
            changed |= renderTonemapSection(exposure);

        outScale = m_renderScale;

        ImGui::End();
        return changed;
    }

    // -------------------------------------------------------------------------

    void RendererPanel::scanHdrFiles(const std::string& directory,
                                      const std::string& defaultFile)
    {
        m_hdrFiles.clear();
        m_selectedHdr = -1;

        const std::filesystem::path searchDir = directory;
        if (!std::filesystem::exists(searchDir)) return;

        for (const auto& entry : std::filesystem::directory_iterator(searchDir))
        {
            if (!entry.is_regular_file()) continue;
            const auto& p = entry.path();
            if (p.extension() == ".hdr" || p.extension() == ".HDR")
                m_hdrFiles.push_back(p.string());
        }

        std::sort(m_hdrFiles.begin(), m_hdrFiles.end());

        if (!defaultFile.empty())
        {
            for (int i = 0; i < static_cast<int>(m_hdrFiles.size()); i++)
            {
                if (m_hdrFiles[i].find(defaultFile) != std::string::npos)
                {
                    m_selectedHdr = i;
                    break;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Private sections
    // -------------------------------------------------------------------------

    bool RendererPanel::renderResolutionSection(int& outW, int& outH)
    {
        bool changed = false;

        // --- Preset combo ---
        const char* currentLabel = k_presets[m_selectedPreset].m_label;
        if (ImGui::BeginCombo("Resolution##Renderer", currentLabel))
        {
            for (int i = 0; i < k_presetCount; i++)
            {
                const bool selected = (m_selectedPreset == i);
                if (ImGui::Selectable(k_presets[i].m_label, selected))
                {
                    m_selectedPreset = i;
                    outW    = k_presets[i].m_width;
                    outH    = k_presets[i].m_height;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // --- Scale slider ---
        // Controls the display zoom inside the viewport window only.
        // Does NOT change the render resolution or trigger an accumulator reset.
        // Range and step match Unity's Game View scale slider behaviour:
        //   min 0.25x (quarter size), max 2.0x (double size), step 0.25.
        // Reference: Unity Manual — "Game view", Scale slider.
        // https://docs.unity3d.com/Manual/GameView.html
        ImGui::SliderFloat("Scale##Renderer", &m_renderScale, 0.25f, 2.0f, "%.2fx");

        // Info line: render resolution + scale = displayed pixel count
        ImGui::TextDisabled("%d x %d  @  %.2fx",
            k_presets[m_selectedPreset].m_width,
            k_presets[m_selectedPreset].m_height,
            m_renderScale);

        return changed;
    }

    // -------------------------------------------------------------------------

    bool RendererPanel::renderAccumulationSection(const int spp, bool& accumulate)
    {
        bool changed = false;

        if (ImGui::Checkbox("Accumulate samples", &accumulate))
            changed = true;

        if (accumulate)
            ImGui::Text("SPP : %d", spp);
        else
            ImGui::TextDisabled("SPP : 1 (off)");

        return changed;
    }

    bool RendererPanel::renderCameraSection() const
    {
        bool changed = false;
        if (ImGui::DragFloat("FOV##Renderer", &m_camera.m_fovDegrees,
                             0.1f, 1.0f, 120.0f, "%.1f deg"))
            changed = true;
        return changed;
    }

    bool RendererPanel::renderSkySection()
    {
        bool changed = false;

        // Build the display label for the current selection.
        const char* currentLabel = (m_selectedHdr < 0)
            ? "None"
            : getFilename(m_hdrFiles[m_selectedHdr]);

        if (ImGui::BeginCombo("HDR##Renderer", currentLabel))
        {
            // "None" entry — index -1
            const bool noneSelected = (m_selectedHdr < 0);
            if (ImGui::Selectable("None", noneSelected))
            {
                m_selectedHdr = -1;
                m_skyDome.unload();
                changed = true;
            }
            if (noneSelected)
                ImGui::SetItemDefaultFocus();

            // File entries
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

        if (m_hdrFiles.empty())
            ImGui::TextDisabled("No .hdr files found.");

        return changed;
    }

    bool RendererPanel::renderTonemapSection(float& exposure)
    {
        bool changed = false;
        if (ImGui::DragFloat("Exposure##Renderer", &exposure,
                             0.01f, 0.0f, 10.0f, "%.2f"))
            changed = true;
        return changed;
    }

    bool RendererPanel::renderDofSection() const
    {
        bool changed = false;

        if (ImGui::DragFloat("FOV##Renderer", &m_camera.m_fovDegrees,
                             0.1f, 1.0f, 120.0f, "%.1f deg"))
            changed = true;

        ImGui::Separator();

        // Depth of Field
        // A lensRadius of 0 disables DoF entirely (pinhole camera).
        // Reference: Shirley & Morley, "Realistic Ray Tracing", 2nd ed., Section 12.2
        if (ImGui::DragFloat("Lens Radius##Renderer", &m_camera.m_lensRadius,
                             0.001f, 0.0f, 0.5f, "%.4f"))
            changed = true;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("0 = pinhole (no DoF)");

        if (ImGui::DragFloat("Focal Distance##Renderer", &m_camera.m_focalDistance,
                             0.01f, 0.01f, 100.0f, "%.3f"))
            changed = true;

        return changed;
    }

    // -------------------------------------------------------------------------

    const char* RendererPanel::getFilename(const std::string& path)
    {
        const auto pos = path.find_last_of("/\\");
        return (pos == std::string::npos) ? path.c_str() : path.c_str() + pos + 1;
    }

}  // namespace rt::management