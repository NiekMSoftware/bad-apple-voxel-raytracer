#include "rt/management/viewport_panel.h"
#include "imgui.h"
#include "tmpl8/template.h"   // brings in glad + GL types

#include <algorithm>   // std::max

namespace rt::management {

    ViewportPanel::~ViewportPanel()
    {
        if (m_texId)
            glDeleteTextures(1, reinterpret_cast<GLuint*>(&m_texId));
    }

    // -------------------------------------------------------------------------

    void ViewportPanel::createOrResizeTexture(const int w, const int h)
    {
        if (m_texId == 0)
            glGenTextures(1, reinterpret_cast<GLuint*>(&m_texId));

        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_texId));

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

        // presentAccumulatorAVX writes 0x00RRGGBB — alpha byte is always 0x00.
        // ImGui's OpenGL3 backend renders with alpha blending enabled, so a
        // texture with alpha=0 would be fully transparent (invisible).
        // GL_TEXTURE_SWIZZLE_A = GL_ONE forces the sampler to return 1.0 for
        // alpha regardless of the stored value, making every pixel fully opaque.
        // Reference: https://www.khronos.org/opengl/wiki/Texture#Swizzle_mask
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);

        // GL_BGRA matches the little-endian byte order of 0x00RRGGBB.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                     GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

        glBindTexture(GL_TEXTURE_2D, 0);

        m_texWidth  = w;
        m_texHeight = h;
    }

    // -------------------------------------------------------------------------

    void ViewportPanel::render(const uint32_t* pixels,
                                const int       renderW,
                                const int       renderH,
                                const float     scale)
    {
        if (!m_bVisible) return;

        // Recreate texture if resolution changed.
        if (m_texId == 0 || m_texWidth != renderW || m_texHeight != renderH)
            createOrResizeTexture(renderW, renderH);

        // Upload current frame.
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_texId));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, renderW, renderH,
                        GL_BGRA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Draw window.
        // NoScrollbar is removed so the user can pan when scale > 1.0.
        // Reference for scrollable child regions in ImGui:
        //   https://github.com/ocornut/imgui/blob/master/imgui_demo.cpp
        //   (search "Scrolling" in the demo source)
        ImGui::SetNextWindowSize(ImVec2(820.0f, 640.0f), ImGuiCond_FirstUseEver);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const bool open = ImGui::Begin("Viewport", &m_bVisible);
        ImGui::PopStyleVar();

        if (!open) { ImGui::End(); return; }

        // --- Compute base fit size (aspect-correct, fills available area) ---
        //
        // Record the content region origin BEFORE moving the cursor.
        // After SetCursorPos the origin moves, so GetContentRegionAvail()
        // would return a smaller region and the offset would be wrong.
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 avail  = ImGui::GetContentRegionAvail();

        const float renderAspect = static_cast<float>(renderW) / static_cast<float>(renderH);

        ImVec2 fitSize = avail;
        if (avail.x / avail.y > renderAspect)
            fitSize.x = avail.y * renderAspect;
        else
            fitSize.y = avail.x / renderAspect;

        // --- Apply display scale on top of the fit size ---
        //
        // scale == 1.0 : image fills the window (default behaviour).
        // scale >  1.0 : image is larger than the window; the window is
        //                scrollable so the user can pan.
        // scale <  1.0 : image shrinks below the window; it is centred
        //                with empty margins around it.
        //
        // std::max(1.0f, ...) prevents the image from collapsing to zero.
        const ImVec2 imgSize(
            std::max(1.0f, fitSize.x * scale),
            std::max(1.0f, fitSize.y * scale));

        // --- Centre the image within the available area ---
        // When scale <= 1 this produces positive margins.
        // When scale >  1 this produces negative offsets (scroll handles the rest).
        const float offsetX = (avail.x - imgSize.x) * 0.5f;
        const float offsetY = (avail.y - imgSize.y) * 0.5f;

        ImGui::SetCursorScreenPos(ImVec2(origin.x + offsetX, origin.y + offsetY));

        // Standard ImGui OpenGL idiom for passing a GL texture ID.
        // Reference: imgui_impl_opengl3.cpp, ImGui::Image() examples.
        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(m_texId)), imgSize);

        // --- Resolution + scale badge — bottom-left corner of the window ---
        const ImVec2 winPos  = ImGui::GetWindowPos();
        const ImVec2 winSize = ImGui::GetWindowSize();
        char badge[48];
        snprintf(badge, sizeof(badge), " %d x %d  @  %.2fx ", renderW, renderH, scale);
        const ImVec2 badgeSize = ImGui::CalcTextSize(badge);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(winPos.x + 4.0f,
                   winPos.y + winSize.y - badgeSize.y - 8.0f),
            ImVec2(winPos.x + badgeSize.x + 8.0f,
                   winPos.y + winSize.y - 4.0f),
            IM_COL32(0, 0, 0, 160));
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(winPos.x + 6.0f,
                   winPos.y + winSize.y - badgeSize.y - 6.0f),
            IM_COL32(200, 200, 200, 255), badge);

        ImGui::End();
    }

}  // namespace rt::management