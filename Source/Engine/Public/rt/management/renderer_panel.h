#pragma once

#include <string>
#include <vector>

namespace rt::core {
    class Camera;
    class SkyDome;
}

namespace rt::management {

    // =========================================================================
    // RendererPanel
    //
    // ImGui window for renderer-level settings:
    //   - Resolution preset selection + render scale slider
    //   - Accumulation toggle + SPP display
    //   - Field of view
    //   - HDR sky dome file selection
    //   - Tone-mapping exposure
    // =========================================================================
    class RendererPanel
    {
    public:
        RendererPanel(core::Camera& camera, core::SkyDome& skyDome);
        ~RendererPanel() = default;

        // Call once per frame from UIManager::render().
        //   spp         : current sample count (read-only display)
        //   exposure    : in/out — caller writes changes back to the renderer
        //   accumulate  : in/out — caller writes changes back to the renderer
        //   outRenderW  : out — new render width  (unchanged if no preset selected)
        //   outRenderH  : out — new render height (unchanged if no preset selected)
        //   outScale    : out — viewport display scale (does not resize renderer)
        // Returns true if any setting changed that requires an accumulator reset.
        bool render(int    spp,
                    float& exposure,
                    bool&  accumulate,
                    int&   outRenderW,
                    int&   outRenderH,
                    float& outScale);

        // Scans 'directory' for .hdr/.HDR files and populates the combo box.
        // 'defaultFile' is matched as a substring to pre-select the loaded sky.
        void scanHdrFiles(const std::string& directory   = "assets/textures",
                          const std::string& defaultFile = "");

        void show()   { m_bVisible = true;        }
        void hide()   { m_bVisible = false;       }
        void toggle() { m_bVisible = !m_bVisible; }
        [[nodiscard]] bool isVisible() const { return m_bVisible; }

    private:
        // ---------------------------------------------------------------------
        // Resolution preset table
        // ---------------------------------------------------------------------
        struct ResolutionPreset
        {
            const char* m_label;
            int         m_width;
            int         m_height;
        };

        static constexpr ResolutionPreset k_presets[] = {
            { "320 x 240",    320,   240 },
            { "640 x 480",    640,   480 },
            { "800 x 600",    800,   600 },
            { "1280 x 720",  1280,   720 },
            { "1920 x 1080", 1920,  1080 },
        };
        static constexpr int k_presetCount = 5;

        // ---------------------------------------------------------------------

        core::Camera&  m_camera;
        core::SkyDome& m_skyDome;

        bool m_bVisible = true;

        // Resolution / scale state
        int   m_selectedPreset = 1;      // default: 640 x 480
        float m_renderScale    = 1.0f;   // viewport display scale, not render resolution

        std::vector<std::string> m_hdrFiles;
        int                      m_selectedHdr = 0;

        // Section helpers — return true when the accumulator should reset
        bool renderResolutionSection(int& outW, int& outH);
        static bool renderAccumulationSection(int spp, bool& accumulate);
        bool        renderCameraSection() const;
        bool        renderSkySection();
        static bool renderTonemapSection(float& exposure);

        static const char* getFilename(const std::string& path);
    };

}  // namespace rt::management