#include "rt/management/light_editor.h"

namespace rt::management {

    bool LightEditor::render(LightManager &lm)
    {
        if (!m_bVisible) return false;
        bool changed = false;

        ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Light Editor", &m_bVisible)) { ImGui::End(); return false; }

        if (ImGui::CollapsingHeader("Directional Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= renderDirectionalSection(lm);
        }

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_DefaultOpen))
            changed |= renderPointSection(lm);

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Spot Lights", ImGuiTreeNodeFlags_DefaultOpen))
            changed |= renderSpotSection(lm);

        ImGui::End();

        return changed;
    }

    bool LightEditor::renderDirectionalSection(LightManager& lm)
    {
        bool changed = false;
        auto& lights = lm.getDirectionalLights();

        if (ImGui::Button("Add##Dir"))
        {
            const rt::lights::DirectionalLight d(float3(-1, 2, 1));
            lm.addDirectionalLight(d);
            m_selDir = static_cast<int>(lights.size()) - 1;
            changed = true;
        }
        if (m_selDir >= 0 && m_selDir < static_cast<int>(lights.size()))
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Remove##Dir"))
            {
                lm.removeDirectionalLight(static_cast<uint>(m_selDir));
                m_selDir = -1;
                changed = true;
            }
            ImGui::PopStyleColor();
        }

        for (int i = 0; i < static_cast<int>(lights.size()); i++)
        {
            char lbl[64];
            snprintf(lbl, sizeof(lbl), "Directional %d", i);
            if (ImGui::Selectable(lbl, m_selDir == i))
            { m_selDir = i; m_selPt = m_selSpot = -1; }
        }

        if (m_selDir >= 0 && m_selDir < static_cast<int>(lights.size()))
        {
            ImGui::Separator();
            auto& l = lights[m_selDir];

            const float3 dir = l.getDirection();
            float d[3] = { dir.x, dir.y, dir.z };
            if (ImGui::DragFloat3("Direction##Dir", d, 0.01f, -1.0f, 1.0f))
                { l.setDirection(float3(d[0], d[1], d[2])); changed = true; }

            const float3 col = l.getColor();
            float c[3] = { col.x, col.y, col.z };
            if (ImGui::ColorEdit3("Color##Dir", c))
                { l.setColor(float3(c[0], c[1], c[2])); changed = true; }

            float intensity = l.getIntensity();
            if (ImGui::DragFloat("Intensity##Dir", &intensity, 0.001f, 0.0f, 5.0f))
                { l.setIntensity(intensity); changed = true; }

            float ar = l.getAngularRadius();
            if (ImGui::SliderFloat("Angular Radius##Dir", &ar, 0.0f, 1.0f))
                { l.setAngularRadius(ar); changed = true; }
        }
        return changed;
    }

    bool LightEditor::renderPointSection(LightManager& lm)
    {
        bool changed = false;
        auto& lights = lm.getPointLights();

        if (ImGui::Button("Add##Pt"))
        {
            lm.addPointLight(rt::lights::PointLight(float3(0.5f), float3(1)));
            m_selPt = static_cast<int>(lights.size()) - 1;
            changed = true;
        }
        if (m_selPt >= 0 && m_selPt < static_cast<int>(lights.size()))
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Remove##Pt"))
            {
                lm.removePointLight(static_cast<uint>(m_selPt));
                m_selPt = -1;
                changed = true;
            }
            ImGui::PopStyleColor();
        }

        for (int i = 0; i < static_cast<int>(lights.size()); i++)
        {
            char lbl[64];
            snprintf(lbl, sizeof(lbl), "Point %d", i);
            if (ImGui::Selectable(lbl, m_selPt == i))
            { m_selPt = i; m_selDir = m_selSpot = -1; }
        }

        if (m_selPt >= 0 && m_selPt < static_cast<int>(lights.size()))
        {
            ImGui::Separator();
            auto& l = lights[m_selPt];

            const float3 pos = l.getPosition();
            float p[3] = { pos.x, pos.y, pos.z };
            if (ImGui::DragFloat3("Position##Pt", p, 0.01f))
                { l.setPosition(float3(p[0], p[1], p[2])); changed = true; }

            const float3 col = l.getColor();
            float c[3] = { col.x, col.y, col.z };
            if (ImGui::ColorEdit3("Color##Pt", c))
                { l.setColor(float3(c[0], c[1], c[2])); changed = true; }

            float intensity = l.getIntensity();
            if (ImGui::DragFloat("Intensity##Pt", &intensity, 0.001f, 0.0f, 5.0f))
                { l.setIntensity(intensity); changed = true; }

            float radius = l.getRadius();
            if (ImGui::DragFloat("Radius##Pt", &radius, 0.001f, 0.0f, 0.5f))
                { l.setRadius(radius); changed = true; }
        }
        return changed;
    }

    bool LightEditor::renderSpotSection(LightManager& lm)
    {
        bool changed = false;
        auto& lights = lm.getSpotLights();

        if (ImGui::Button("Add##Spot"))
        {
            lm.addSpotLight(rt::lights::SpotLight(float3(0.5f), float3(0, -1, 0)));
            m_selSpot = static_cast<int>(lights.size()) - 1;
            changed = true;
        }
        if (m_selSpot >= 0 && m_selSpot < static_cast<int>(lights.size()))
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Remove##Spot"))
            {
                lm.removeSpotLight(static_cast<uint>(m_selSpot));
                m_selSpot = -1;
                changed = true;
            }
            ImGui::PopStyleColor();
        }

        for (int i = 0; i < static_cast<int>(lights.size()); i++)
        {
            char lbl[64];
            snprintf(lbl, sizeof(lbl), "Spot %d", i);
            if (ImGui::Selectable(lbl, m_selSpot == i))
            { m_selSpot = i; m_selDir = m_selPt = -1; }
        }

        if (m_selSpot >= 0 && m_selSpot < static_cast<int>(lights.size()))
        {
            ImGui::Separator();
            auto& l = lights[m_selSpot];

            const float3 pos = l.getPosition();
            float p[3] = { pos.x, pos.y, pos.z };
            if (ImGui::DragFloat3("Position##Spot", p, 0.01f))
                { l.setPosition(float3(p[0], p[1], p[2])); changed = true; }

            float pitch = l.getPitch();
            float yaw   = l.getYaw();
            if (ImGui::DragFloat("Pitch##Spot", &pitch, 0.01f, -90.0f, 90.0f))
                { l.setPitch(pitch); changed = true; }
            if (ImGui::DragFloat("Yaw##Spot",   &yaw, 0.01f,  -180.0f, 180.0f))
                { l.setYaw(yaw); changed = true; }

            const float3 col = l.getColor();
            float c[3] = { col.x, col.y, col.z };
            if (ImGui::ColorEdit3("Color##Spot", c))
                { l.setColor(float3(c[0], c[1], c[2])); changed = true; }

            float intensity = l.getIntensity();
            if (ImGui::DragFloat("Intensity##Spot", &intensity, 0.001f, 0.0f, 5.0f))
                { l.setIntensity(intensity); changed = true; }

            float angle = l.getSpotAngle();
            if (ImGui::DragFloat("Spot Angle##Spot", &angle, 0.01f, 1.0f, 89.0f))
                { l.setSpotAngle(angle); changed = true; }

            float soft = l.getSoftBandRatio();
            if (ImGui::DragFloat("Soft Band##Spot", &soft, 0.01f, 0.0f, 1.0f))
                { l.setSoftBandRatio(soft); changed = true; }

            float radius = l.getRadius();
            if (ImGui::DragFloat("Radius##Spot", &radius, 0.01f, 0.0f, 0.5f))
                { l.setRadius(radius); changed = true; }
        }
        return changed;
    }

}  // namespace Tmpl8::Management