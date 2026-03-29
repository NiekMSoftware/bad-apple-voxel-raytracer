#pragma once

#include "rt/scene/scene.h"
#include "rt/rendering/bit_packer.h"
#include "audio_player.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <cstdio>

namespace demo {

    class BadApplePlayer
    {
    public:
        static constexpr uint  FRAME_WIDTH    = 512;
        static constexpr uint  FRAME_HEIGHT   = 512;
        static constexpr uint  FRAME_SIZE     = (FRAME_WIDTH * FRAME_HEIGHT) / 8;
        static constexpr float TARGET_FPS     = 30.0f;
        static constexpr float FRAME_TIME     = 1.0f / TARGET_FPS;

        static constexpr uint WALL_Z         = WORLDSIZE / 2;
        static constexpr uint WALL_THICKNESS = 1;

        BadApplePlayer() = default;
        ~BadApplePlayer() = default;

        // ----------------------------------------------------------------
        // load
        // ----------------------------------------------------------------
        bool load(const std::string& videoPath,
                  const std::string& audioPath = "")
        {
            FILE* f = fopen(videoPath.c_str(), "rb");
            if (!f) {
                printf("BadApplePlayer: failed to open '%s'\n", videoPath.c_str());
                return false;
            }

            fseek(f, 0, SEEK_END);
            const long fileSize = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (fileSize <= 0) {
                printf("BadApplePlayer: empty file\n");
                fclose(f);
                return false;
            }

            long headerSize = 0;
            long dataSize   = fileSize;

            if (fileSize % FRAME_SIZE != 0) {
                if ((fileSize - 4) % FRAME_SIZE == 0) {
                    headerSize = 4;
                    dataSize   = fileSize - 4;
                    printf("BadApplePlayer: detected 4-byte header, skipping\n");
                } else {
                    printf("BadApplePlayer: file size %ld is not a valid frame stream\n", fileSize);
                    fclose(f);
                    return false;
                }
            }

            if (headerSize > 0)
                fseek(f, headerSize, SEEK_SET);

            m_frameCount = static_cast<uint>(dataSize / FRAME_SIZE);
            m_vFrameData.resize(dataSize);
            fread(m_vFrameData.data(), 1, dataSize, f);
            fclose(f);

            m_vPrevFrame.assign(FRAME_SIZE, 0);
            m_currentFrame = 0;
            m_elapsedTime  = 0.0f;
            m_bPlaying     = true;
            m_bLoaded      = true;
            m_bFirstFrame  = true;

            printf("BadApplePlayer: loaded %u frames (%ux%u, 1bpp) from '%s' "
                   "(%.1f seconds at %.0f fps)\n",
                   m_frameCount, FRAME_WIDTH, FRAME_HEIGHT, videoPath.c_str(),
                   static_cast<float>(m_frameCount) / TARGET_FPS, TARGET_FPS);

            if (!audioPath.empty())
            {
                if (m_audio.load(audioPath))
                {
                    m_audio.setLooping(m_bLooping);
                    m_audio.play();
                }
            }

            return true;
        }

        // ----------------------------------------------------------------
        // update
        // ----------------------------------------------------------------
        bool update(rt::scene::Scene& scene, const float deltaTime)
        {
            if (!m_bLoaded || !m_bPlaying) return false;

            float playbackTime;
            if (m_audio.isLoaded())
                playbackTime = m_audio.getCursorSeconds();
            else
            {
                m_elapsedTime += deltaTime;
                playbackTime   = m_elapsedTime;
            }

            uint targetFrame = static_cast<uint>(playbackTime / FRAME_TIME);

            if (targetFrame >= m_frameCount)
            {
                if (m_bLooping)
                {
                    clearDisplay(scene);
                    m_elapsedTime  = 0.0f;
                    targetFrame    = 0;
                    m_currentFrame = 0;
                    m_bFirstFrame  = true;
                    m_bRestarted   = true;
                    m_vPrevFrame.assign(FRAME_SIZE, 0);
                    m_audio.seekTo(0.0f);
                }
                else
                {
                    m_bPlaying = false;
                    m_audio.pause();
                    return false;
                }
            }

            if (targetFrame == m_currentFrame && !m_bFirstFrame)
                return false;

            m_currentFrame = targetFrame;
            m_bFirstFrame  = false;

            writeFrame(scene, &m_vFrameData[
                static_cast<size_t>(m_currentFrame) * FRAME_SIZE]);
            return true;
        }

        // ----------------------------------------------------------------
        // renderUI
        //
        // Timeline window with:
        //   - MM:SS.f current timestamp and total duration
        //   - Scrubber (SliderInt over frame index) -- drag to seek
        //   - Play/Pause, Restart, Loop, Volume
        // ----------------------------------------------------------------
        void renderUI(rt::scene::Scene& scene)
        {
            if (!m_bLoaded || !m_bVisible) return;

            ImGui::SetNextWindowSize(ImVec2(480.0f, 140.0f), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Bad Apple", &m_bVisible)) { ImGui::End(); return; }

            // ---- Timestamp display ----
            const float currentSec  = static_cast<float>(m_currentFrame) * FRAME_TIME;
            const float totalSec    = static_cast<float>(m_frameCount)   * FRAME_TIME;

            char currentStr[20];
            char totalStr[20];
            formatTime(currentSec, currentStr, sizeof(currentStr));
            formatTime(totalSec,   totalStr,   sizeof(totalStr));

            ImGui::Text("%s / %s", currentStr, totalStr);
            ImGui::SameLine();
            ImGui::TextDisabled("  frame %u / %u  |  %.0f fps",
                m_currentFrame, m_frameCount, TARGET_FPS);

            // ---- Scrubber ----
            //
            // DragFloat gives a configurable drag speed so the user can scrub
            // slowly at 1ms per pixel.  SliderFloat mapped the entire duration
            // to the widget width which was far too fast for a 3+ minute video.
            //
            // Drag speed:
            //   Default (no modifier)  : 0.033s per pixel  (~1 frame at 30fps)
            //   Ctrl held while typing : exact value entry in seconds
            //
            // The progress bar underneath is drawn manually from the same
            // scrubSec value so it always reflects the true playback position.
            float scrubSec = m_audio.isLoaded()
                ? m_audio.getCursorSeconds()
                : m_elapsedTime;
            scrubSec = (scrubSec < 0.0f) ? 0.0f
                     : (scrubSec > totalSec) ? totalSec
                     : scrubSec;

            // Progress bar drawn behind the DragFloat using the window draw list.
            {
                const ImVec2 barPos  = ImGui::GetCursorScreenPos();
                const float  barW    = ImGui::GetContentRegionAvail().x;
                const float  barH    = ImGui::GetFrameHeight();
                const float  filled  = totalSec > 0.0f
                    ? (scrubSec / totalSec) * barW : 0.0f;

                // Dark background
                ImGui::GetWindowDrawList()->AddRectFilled(
                    barPos,
                    ImVec2(barPos.x + barW, barPos.y + barH),
                    IM_COL32(40, 40, 40, 255));

                // Filled portion
                if (filled > 0.0f)
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        barPos,
                        ImVec2(barPos.x + filled, barPos.y + barH),
                        IM_COL32(80, 120, 180, 200));

                // Playhead marker
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(barPos.x + filled, barPos.y),
                    ImVec2(barPos.x + filled, barPos.y + barH),
                    IM_COL32(255, 200, 60, 230), 2.0f);
            }

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::DragFloat("##scrub", &scrubSec, 0.033f, 0.0f, totalSec, "%.3f"))
            {
                const uint targetFrame = static_cast<uint>(scrubSec / FRAME_TIME);
                seekToFrame(scene, targetFrame < m_frameCount ? targetFrame : m_frameCount - 1);
                m_audio.seekTo(scrubSec);
                m_elapsedTime = scrubSec;
            }

            // ---- Transport controls ----
            if (m_bPlaying) {
                if (ImGui::Button("Pause"))   pausePlayback();
            } else {
                if (ImGui::Button("Play"))    resumePlayback();
            }

            ImGui::SameLine();
            if (ImGui::Button("Restart"))
                restart(scene);

            ImGui::SameLine();
            if (ImGui::Checkbox("Loop", &m_bLooping))
                m_audio.setLooping(m_bLooping);

            if (m_audio.isLoaded())
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120.0f);
                ImGui::SliderFloat("Volume", &m_volume, 0.0f, 1.0f, "%.2f");
                m_audio.setVolume(m_volume);
            }

            ImGui::End();
        }

        // ----------------------------------------------------------------
        // Playback controls
        // ----------------------------------------------------------------
        void pausePlayback()
        {
            m_bPlaying = false;
            m_audio.pause();
        }

        void resumePlayback()
        {
            m_bPlaying = true;
            m_audio.play();
        }

        void restart(rt::scene::Scene& scene)
        {
            clearDisplay(scene);
            m_elapsedTime  = 0.0f;
            m_currentFrame = 0;
            m_bPlaying     = true;
            m_bFirstFrame  = true;
            m_bRestarted   = true;
            m_vPrevFrame.assign(FRAME_SIZE, 0);
            m_audio.seekTo(0.0f);
            m_audio.play();
        }

        void setLooping(const bool loop)
        {
            m_bLooping = loop;
            m_audio.setLooping(loop);
        }

        void show()   { m_bVisible = true;        }
        void hide()   { m_bVisible = false;       }
        void toggle() { m_bVisible = !m_bVisible; }

        [[nodiscard]] bool  isLoaded()        const { return m_bLoaded;      }
        [[nodiscard]] bool  isPlaying()       const { return m_bPlaying;     }
        [[nodiscard]] bool  isVisible()       const { return m_bVisible;     }
        [[nodiscard]] uint  getCurrentFrame() const { return m_currentFrame; }
        [[nodiscard]] uint  getFrameCount()   const { return m_frameCount;   }

        // Returns the current playback position in seconds -- the same value
        // that update() uses internally to drive the frame counter.
        // When audio is loaded this is the audio cursor; otherwise it is the
        // elapsed time accumulator.  Use this to sync external systems (e.g.
        // the camera spline) to the exact same timeline as the video.
        [[nodiscard]] float getPlaybackTime() const
        {
            if (!m_bLoaded) return 0.0f;
            if (m_audio.isLoaded()) return m_audio.getCursorSeconds();
            return m_elapsedTime;
        }

        // consumeRestart -- returns true and clears the flag whenever
        // restart() was called or the video looped since the last call.
        bool consumeRestart()
        {
            if (!m_bRestarted) return false;
            m_bRestarted = false;
            return true;
        }
        [[nodiscard]] float getProgress()     const
        {
            return m_frameCount > 0
                ? static_cast<float>(m_currentFrame) / static_cast<float>(m_frameCount)
                : 0.0f;
        }

        uint m_materialId = 0;

    private:
        // ----------------------------------------------------------------
        // formatTime -- writes "MM:SS.fff" into buf (e.g. "03:45.033")
        // ----------------------------------------------------------------
        static void formatTime(const float seconds, char* buf, const int bufSize)
        {
            const int totalMs  = static_cast<int>(seconds * 1000.0f);
            const int ms       = totalMs % 1000;
            const int totalSecs = totalMs / 1000;
            const int secs     = totalSecs % 60;
            const int mins     = totalSecs / 60;
            snprintf(buf, bufSize, "%02d:%02d.%03d", mins, secs, ms);
        }

        // ----------------------------------------------------------------
        // seekToFrame
        //
        // Jumps the video and audio to the requested frame.
        // The wall must be cleared and m_vPrevFrame reset so that
        // writeFrame does a full redraw from the new position rather
        // than a stale delta from the old position.
        // ----------------------------------------------------------------
        void seekToFrame(rt::scene::Scene& scene, const uint frame)
        {
            if (!m_bLoaded || frame >= m_frameCount) return;

            // Clear the wall and reset the delta buffer so writeFrame
            // redraws everything from the new frame correctly.
            clearDisplay(scene);
            m_vPrevFrame.assign(FRAME_SIZE, 0);

            m_currentFrame = frame;
            m_bFirstFrame  = true;

            // Sync elapsed time fallback
            m_elapsedTime = static_cast<float>(frame) * FRAME_TIME;

            // Sync audio
            m_audio.seekTo(m_elapsedTime);

            // Write the new frame immediately so the wall does not stay
            // blank until the next update() call.
            writeFrame(scene, &m_vFrameData[
                static_cast<size_t>(m_currentFrame) * FRAME_SIZE]);

            m_bFirstFrame = false;
        }

        // ----------------------------------------------------------------
        static bool getBit(const uint8_t* data, const uint px, const uint py)
        {
            const uint bitIdx   = px + py * FRAME_WIDTH;
            const uint byteIdx  = bitIdx / 8;
            const uint bitShift = 7 - (bitIdx % 8);
            return (data[byteIdx] >> bitShift) & 1;
        }

        static void clearDisplay(rt::scene::Scene& scene)
        {
            for (uint py = 0; py < FRAME_HEIGHT; py++)
                for (uint px = 0; px < FRAME_WIDTH; px++)
                {
                    const uint wy = FRAME_HEIGHT - 1 - py;
                    for (uint z = WALL_Z; z < WALL_Z + WALL_THICKNESS; z++)
                        scene.set(px, wy, z, 0);
                }
        }

        void writeFrame(rt::scene::Scene& scene, const uint8_t* frame)
        {
            for (uint py = 0; py < FRAME_HEIGHT; py++)
                for (uint px = 0; px < FRAME_WIDTH; px++)
                {
                    const bool curOn  = getBit(frame,               px, py);
                    const bool prevOn = getBit(m_vPrevFrame.data(),  px, py);

                    if (curOn == prevOn) continue;

                    const uint wy = FRAME_HEIGHT - 1 - py;

                    if (curOn) {
                        const uint voxel = rt::rendering::packVoxel(
                            m_materialId, 255, 255, 255);
                        for (uint z = WALL_Z; z < WALL_Z + WALL_THICKNESS; z++)
                            scene.set(px, wy, z, voxel);
                    } else {
                        for (uint z = WALL_Z; z < WALL_Z + WALL_THICKNESS; z++)
                            scene.set(px, wy, z, 0);
                    }
                }

            memcpy(m_vPrevFrame.data(), frame, FRAME_SIZE);
        }

        // ----------------------------------------------------------------
        std::vector<uint8_t> m_vFrameData;
        std::vector<uint8_t> m_vPrevFrame;

        uint  m_frameCount   = 0;
        uint  m_currentFrame = 0;
        float m_elapsedTime  = 0.0f;
        float m_volume       = 1.0f;

        bool m_bLoaded     = false;
        bool m_bPlaying    = false;
        bool m_bLooping    = true;
        bool m_bFirstFrame = true;
        bool m_bVisible    = true;
        bool m_bRestarted  = false;

        AudioPlayer m_audio;
    };

}  // namespace demo