#include "rt/rendering/material_editor.h"
#include "imgui.h"

namespace rt::rendering {

    bool MaterialEditor::render(MaterialManager& materialManager)
    {
        if (!m_bVisible) return false;

        ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);

        bool changed = false;

        if (!ImGui::Begin("Material Editor", &m_bVisible)) {
            ImGui::End();
            return false;
        }

        // Material selection list
        renderMaterialList(materialManager);

        ImGui::Separator();

        // Property editor for selected material
        changed = renderPropertyEditor(materialManager);

        ImGui::End();
        return changed;
    }

    void MaterialEditor::renderMaterialList(MaterialManager& materialManager)
    {
        ImGui::Text("Materials");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select a material to edit its properties.\n"
                              "Changes are applied in real-time.");
        }

        // Calculate height for the list box (show ~8 items)
        const float itemHeight = ImGui::GetTextLineHeightWithSpacing();
        const float listHeight = itemHeight * 8;

        if (ImGui::BeginListBox("##MaterialList", ImVec2(-FLT_MIN, listHeight))) {
            for (uint i = 0; i < materialManager.getMaterialCount(); i++) {
                if (!materialManager.isMaterialActive(i)) continue;

                const Material mat = materialManager.getMaterial(i);

                // Format: "ID: Name [M:0.0 R:0.0]" showing metallic and roughness
                char label[128];
                snprintf(label, sizeof(label), "%u: %s [M:%.1f R:%.1f]",
                         i,
                         MaterialManager::getMaterialTypeName(i),
                         mat.m_metallic,
                         mat.m_roughness);

                const bool isSelected = (materialManager.m_selectedMaterialIndex == i);

                // Color indicator before the selectable
                ImVec4 matColor(mat.m_baseColor.x, mat.m_baseColor.y, mat.m_baseColor.z, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Header, matColor);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                    ImVec4(mat.m_baseColor.x * 1.2f, mat.m_baseColor.y * 1.2f, mat.m_baseColor.z * 1.2f, 1.0f));

                if (ImGui::Selectable(label, isSelected)) {
                    materialManager.m_selectedMaterialIndex = i;
                }

                ImGui::PopStyleColor(2);

                // Set initial focus
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndListBox();
        }
    }

    bool MaterialEditor::renderPropertyEditor(MaterialManager& materialManager)
    {
        const uint selectedIdx = materialManager.m_selectedMaterialIndex;

        if (!materialManager.isMaterialActive(selectedIdx)) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No material selected");
            return false;
        }

        Material material = materialManager.getMaterial(selectedIdx);
        bool hasChanged = false;

        // Header with material name
        ImGui::Text("Editing: %s (ID: %u)",
                    MaterialManager::getMaterialTypeName(selectedIdx),
                    selectedIdx);

        ImGui::Spacing();

        // Preview
        renderMaterialPreview(material);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // === Base Color ===
        ImGui::Text("Base Color");
        float color[3] = {material.m_baseColor.x, material.m_baseColor.y, material.m_baseColor.z};
        if (ImGui::ColorEdit3("##BaseColor", color,
                              ImGuiColorEditFlags_PickerHueWheel |
                              ImGuiColorEditFlags_DisplayRGB)) {
            material.m_baseColor = float3(color[0], color[1], color[2]);
            hasChanged = true;
        }

        ImGui::Spacing();

        // === Metallic ===
        ImGui::Text("Metallic");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0.0 = Dielectric (plastic, wood, etc.)\n"
                              "1.0 = Pure metal (gold, silver, etc.)");
        }
        if (ImGui::SliderFloat("##Metallic", &material.m_metallic, 0.0f, 1.0f, "%.2f")) {
            hasChanged = true;
        }

        // === Roughness ===
        ImGui::Text("Roughness");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0.0 = Perfectly smooth (mirror-like)\n"
                              "1.0 = Completely rough (diffuse)\n\n"
                              "TIP: Values < 0.5 cause expensive reflections!");
        }

        // Color the slider based on performance impact
        if (material.m_roughness < 0.5f) {
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.3f, 0.0f, 1.0f));
        }

        if (ImGui::SliderFloat("##Roughness", &material.m_roughness, 0.0f, 1.0f, "%.2f")) {
            hasChanged = true;
        }

        if (material.m_roughness < 0.5f) {
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(!)" );
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Low roughness = more reflections = lower FPS");
            }
        }

        // === Anisotropy ===
        ImGui::Text("Anisotropy");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Controls the direction of specular highlights.\n"
                              "0.0 = Perfectly specular (no anisotropy)\n"
                              "1.0 = Perfectly diffuse (full anisotropy)");
        }
        if (ImGui::SliderFloat("##Anisotropy", &material.m_anisotropy, -1.0f, 1.0f, "%.2f")) {
            hasChanged = true;
        }

        // === Index of Refraction ===
        ImGui::Text("Index of Refraction (IOR)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Common values:\n"
                              "  Air: 1.0\n"
                              "  Water: 1.33\n"
                              "  Glass: 1.45-1.65\n"
                              "  Diamond: 2.42");
        }
        if (ImGui::SliderFloat("##IOR", &material.m_indexOfRefraction, 1.0f, 2.5f, "%.2f")) {
            hasChanged = true;
        }

        // === Transparency ===
        ImGui::Text("Transparency");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0.0 = Fully opaque\n"
                              "1.0 = Fully transparent\n\n"
                              "WARNING: Transparency is expensive!");
        }

        // Warn about transparency performance
        if (material.m_transparency > 0.0f) {
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.1f, 0.1f, 1.0f));
        }

        if (ImGui::SliderFloat("##Transparency", &material.m_transparency, 0.0f, 1.0f, "%.2f")) {
            hasChanged = true;
        }

        if (material.m_transparency > 0.0f) {
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "(!!)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Transparency requires tracing refraction rays.\n"
                                  "This significantly impacts performance!");
            }
        }

        // === Light Absorption Density ===
        ImGui::Text("Absorption Density");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Controls Beer-Lambert absorption rate.\n"
                              "0.0 = No color absorption\n"
                              "1.0 = Default\n"
                              "Higher = glass absorbs color faster with thickness");
        }
        if (ImGui::SliderFloat("##AbsorptionDensity", &material.m_absorptionDensity, 0.0f, 10.0f, "%.2f")) {
            hasChanged = true;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Presets
        renderPresetButtons(material, hasChanged);

        // Apply changes
        if (hasChanged) {
            materialManager.updateMaterial(selectedIdx, material);
        }

        return hasChanged;
    }

    void MaterialEditor::renderPresetButtons(Material& material, bool& hasChanged)
    {
        ImGui::Text("Quick Presets");

        constexpr float buttonWidth = 70.0f;

        // Row 1: Basic materials
        if (ImGui::Button("Matte", ImVec2(buttonWidth, 0))) {
            material.m_metallic = 0.0f;
            material.m_roughness = 0.9f;
            material.m_anisotropy = 0.0f;
            material.m_transparency = 0.0f;
            material.m_indexOfRefraction = 1.5f;
            hasChanged = true;
        }
        ImGui::SameLine();

        if (ImGui::Button("Plastic", ImVec2(buttonWidth, 0))) {
            material.m_metallic = 0.0f;
            material.m_roughness = 0.4f;
            material.m_anisotropy = 0.0f;
            material.m_transparency = 0.0f;
            material.m_indexOfRefraction = 1.46f;
            hasChanged = true;
        }
        ImGui::SameLine();

        if (ImGui::Button("Ceramic", ImVec2(buttonWidth, 0))) {
            material.m_metallic = 0.0f;
            material.m_roughness = 0.3f;
            material.m_anisotropy = 0.0f;
            material.m_transparency = 0.0f;
            material.m_indexOfRefraction = 1.5f;
            hasChanged = true;
        }
        ImGui::SameLine();

        if (ImGui::Button("Rubber", ImVec2(buttonWidth, 0))) {
            material.m_metallic = 0.0f;
            material.m_roughness = 0.95f;
            material.m_anisotropy = 0.0f;
            material.m_transparency = 0.0f;
            material.m_indexOfRefraction = 1.5f;
            hasChanged = true;
        }

        // Row 2: Metals
        if (ImGui::Button("Chrome", ImVec2(buttonWidth, 0))) {
            material.m_baseColor = float3(0.95f, 0.93f, 0.88f);
            material.m_metallic = 1.0f;
            material.m_roughness = 0.05f;
            material.m_anisotropy = 0.0f;
            material.m_transparency = 0.0f;
            hasChanged = true;
        }
        ImGui::SameLine();

        if (ImGui::Button("Gold", ImVec2(buttonWidth, 0))) {
            material.m_baseColor = float3(1.0f, 0.76f, 0.33f);
            material.m_metallic = 1.0f;
            material.m_roughness = 0.2f;
            material.m_anisotropy = 0.0f;
            material.m_transparency = 0.0f;
            hasChanged = true;
        }
        ImGui::SameLine();

        if (ImGui::Button("Copper", ImVec2(buttonWidth, 0))) {
            material.m_baseColor = float3(0.95f, 0.64f, 0.54f);
            material.m_metallic = 1.0f;
            material.m_roughness = 0.25f;
            material.m_anisotropy = 0.0f;
            material.m_transparency = 0.0f;
            hasChanged = true;
        }
        ImGui::SameLine();

        if (ImGui::Button("Brushed", ImVec2(buttonWidth, 0))) {
            material.m_metallic = 1.0f;
            material.m_roughness = 0.35f;
            material.m_anisotropy = 0.75f;
            material.m_transparency = 0.0f;
            hasChanged = true;
        }

        // Row 3: Transparent
        if (ImGui::Button("Glass", ImVec2(buttonWidth, 0))) {
            material.m_baseColor = float3(1.0f, 1.0f, 1.0f);
            material.m_metallic = 0.0f;
            material.m_roughness = 0.02f;
            material.m_anisotropy = 0.0f;
            material.m_transparency = 0.95f;
            material.m_indexOfRefraction = 1.52f;
            hasChanged = true;
        }
        ImGui::SameLine();

        if (ImGui::Button("Water", ImVec2(buttonWidth, 0))) {
            material.m_baseColor = float3(0.8f, 0.9f, 1.0f);
            material.m_metallic = 0.0f;
            material.m_roughness = 0.05f;
            material.m_anisotropy = 0.0f;
            material.m_transparency = 0.9f;
            material.m_indexOfRefraction = 1.33f;
            hasChanged = true;
        }
        ImGui::SameLine();

        if (ImGui::Button("Diamond", ImVec2(buttonWidth, 0))) {
            material.m_baseColor = float3(1.0f, 1.0f, 1.0f);
            material.m_metallic = 0.0f;
            material.m_roughness = 0.0f;
            material.m_anisotropy = 0.0f;
            material.m_transparency = 0.98f;
            material.m_indexOfRefraction = 2.42f;
            hasChanged = true;
        }
        ImGui::SameLine();

        if (ImGui::Button("Frosted", ImVec2(buttonWidth, 0))) {
            material.m_metallic = 0.0f;
            material.m_roughness = 0.5f;
            material.m_anisotropy = 0.0f;
            material.m_transparency = 0.7f;
            material.m_indexOfRefraction = 1.5f;
            hasChanged = true;
        }
    }

    void MaterialEditor::renderMaterialPreview(const Material& material)
    {
        // Visual representation of material properties
        constexpr float previewSize = 60.0f;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Background
        drawList->AddRectFilled(
            pos,
            ImVec2(pos.x + previewSize, pos.y + previewSize),
            IM_COL32(30, 30, 30, 255)
        );

        // Material color sphere approximation (simple gradient)
        const ImU32 baseCol = IM_COL32(
            static_cast<int>(material.m_baseColor.x * 255),
            static_cast<int>(material.m_baseColor.y * 255),
            static_cast<int>(material.m_baseColor.z * 255),
            static_cast<int>((1.0f - material.m_transparency * 0.5f) * 255)
        );

        // Center of preview
        const ImVec2 center(pos.x + previewSize * 0.5f, pos.y + previewSize * 0.5f);
        constexpr float radius = previewSize * 0.4f;

        // Draw sphere approximation
        drawList->AddCircleFilled(center, radius, baseCol);

        // Highlight based on roughness (smoother = brighter highlight)
        if (material.m_roughness < 0.7f) {
            const float highlightIntensity = (1.0f - material.m_roughness) * 0.8f;
            const ImU32 highlightCol = IM_COL32(
                255, 255, 255,
                static_cast<int>(highlightIntensity * 200 * (material.m_metallic * 0.5f + 0.5f))
            );
            const ImVec2 highlightPos(center.x - radius * 0.3f, center.y - radius * 0.3f);
            drawList->AddCircleFilled(highlightPos, radius * 0.25f * (1.0f - material.m_roughness), highlightCol);
        }

        // Metallic indicator ring
        if (material.m_metallic > 0.5f) {
            drawList->AddCircle(center, radius + 2, IM_COL32(255, 215, 0, 150), 0, 2.0f);
        }

        // Transparency indicator (dashed circle)
        if (material.m_transparency > 0.1f) {
            drawList->AddCircle(center, radius + 4,
                               IM_COL32(100, 150, 255, static_cast<int>(material.m_transparency * 200)),
                               0, 1.5f);
        }

        // Reserve space
        ImGui::Dummy(ImVec2(previewSize, previewSize));

        // Property summary next to preview
        ImGui::SameLine();
        ImGui::BeginGroup();

        if (material.m_metallic > 0.5f) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "Metallic");
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Dielectric");
        }

        if (material.m_roughness < 0.3f) {
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Smooth");
        } else if (material.m_roughness > 0.7f) {
            ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.4f, 1.0f), "Rough");
        } else {
            ImGui::Text("Semi-glossy");
        }

        if (material.m_transparency > 0.5f) {
            ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Transparent");
        } else if (material.m_transparency > 0.0f) {
            ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.8f, 1.0f), "Translucent");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Opaque");
        }

        ImGui::EndGroup();
    }

}  // Tmpl8