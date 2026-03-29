#pragma once

// ============================================================================
// BlueNoise
//
// Wraps a 64x64 two-channel blue noise texture (LDR_RG01_0.png) and exposes
// a single sample() call that returns a float2 jitter in [0, 1).
//
// The texture is tiled across the screen using (x & 63) + (y & 63) * 64.
// Red and green channels provide the two independent noise values.
//
// Temporal animation uses the golden ratio (0.61803399), added to the base
// value each frame and wrapped with fracf().  This produces a low-discrepancy
// sequence per pixel that retains the blue-noise spatial distribution across
// frames.
//
// Reference:
//   Bikker, J. (2024). Ray Tracing with Voxels in C++ Series - Part 4.
//   https://jacco.ompf2.com/2024/05/15/ray-tracing-with-voxels-in-c-series-part-4/
// ============================================================================

#include "stb_image.h"
#include "tmpl8/template.h"

namespace rt::core {

    struct BlueNoise
    {
        ~BlueNoise() {
            delete m_surface;
        }

        // ------------------------------------------------------------------
        // load
        //
        // Loads the blue noise texture from disk.  Must be called before
        // any sample() calls.  Returns true on success.
        // ------------------------------------------------------------------
        bool load(const char* path)
        {
            m_surface = new Tmpl8::Surface(path);
            if (!m_surface->pixels || m_surface->width == 0)
            {
                printf("BlueNoise: failed to load '%s'\n", path);
                return false;
            }
            printf("BlueNoise: loaded '%s' (%dx%d)\n",
                   path, m_surface->width, m_surface->height);
            return true;
        }

        // ------------------------------------------------------------------
        // sample
        //
        // Returns a float2 jitter in [0, 1) for screen pixel (x, y) at the
        // given frame index.
        // ------------------------------------------------------------------
        [[nodiscard]] float2 sample(const int x, const int y,
                                    const uint frameIndex) const
        {
            if (!m_surface->pixels)
                return { 0.5f, 0.5f };  // fallback: pixel centre, no jitter

            const int  idx   = (x & 63) + (y & 63) * 64;
            const uint pixel = m_surface->pixels[idx];

            const float r0 = static_cast<float>((pixel >> 16) & 0xFF) / 256.0f;
            const float r1 = static_cast<float>((pixel >>  8) & 0xFF) / 256.0f;

            // Animate: add golden ratio each frame, keep fractional part
            constexpr float k_goldenRatio = 0.61803399f;
            const auto frame = static_cast<float>(frameIndex);

            return {
                fracf(r0 + frame * k_goldenRatio),
                fracf(r1 + frame * k_goldenRatio)
            };
        }

        [[nodiscard]] bool isLoaded() const
        {
            return m_surface->pixels != nullptr;
        }

    private:
        Tmpl8::Surface* m_surface = nullptr;
    };

}  // namespace rt::core