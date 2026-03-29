#pragma once

#include <tmpl8/template.h>

namespace rt::management {

    // =========================================================================
    // ViewportPanel
    //
    // Displays the renderer's pixel buffer as an ImGui image window.
    // Owns a single OpenGL texture that is re-uploaded every frame.
    //
    // The image is letterboxed/pillarboxed inside the available content area
    // so the render aspect ratio is always preserved regardless of how the
    // user resizes the docked window.
    //
    // A 'scale' parameter zooms the displayed image on top of the fit size.
    // When scale > 1.0 the image exceeds the content area and the window
    // becomes scrollable, allowing the user to pan around the zoomed image.
    // When scale < 1.0 the image shrinks and is centred with empty margins.
    //
    // Pixel format expected: 0x00RRGGBB (Tmpl8 / RGBF32_to_RGB8 output).
    // Uploaded to GL as GL_BGRA so channels map correctly without any swizzle.
    // =========================================================================
    class ViewportPanel
    {
    public:
        ViewportPanel()  = default;
        ~ViewportPanel();

        // Upload pixels and draw the ImGui window.
        // Must be called between ImGui::NewFrame() and ImGui::Render().
        //   scale : display zoom multiplier applied on top of the fit-to-window
        //           size.  1.0 = fills the window; 2.0 = twice as large (scrollable).
        void render(const uint32_t* pixels, int renderW, int renderH,
                    float scale = 1.0f);

        void show()   { m_bVisible = true;        }
        void hide()   { m_bVisible = false;       }
        void toggle() { m_bVisible = !m_bVisible; }
        [[nodiscard]] bool isVisible() const { return m_bVisible; }

    private:
        // GLuint stored as uint to keep GL headers out of this header.
        uint m_texId     = 0;
        int  m_texWidth  = 0;
        int  m_texHeight = 0;
        bool m_bVisible  = true;

        void createOrResizeTexture(int w, int h);
    };

}  // namespace rt::management