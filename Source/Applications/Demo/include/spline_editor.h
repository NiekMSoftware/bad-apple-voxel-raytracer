#pragma once

// ============================================================================
// SplineEditor
//
// Self-contained ImGui window for editing the two camera splines.
//
// Binary file format ("camera_spline.bin"):
//
//   [4 bytes]  uint32_t  keyframe count  N
//   [N * 28 bytes]  per keyframe:
//     float  time        (4 bytes)
//     float  pos.x       (4 bytes)
//     float  pos.y       (4 bytes)
//     float  pos.z       (4 bytes)
//     float  target.x    (4 bytes)
//     float  target.y    (4 bytes)
//     float  target.z    (4 bytes)
//   [1 byte]   uint8_t   loop flag (0 or 1)  -- appended after keyframes
//
// The loop flag is appended at the end for backward compatibility.
// If the file was saved without it (old format), the flag defaults to 0
// (no looping).  Detection is by file-size: if exactly
// 4 + N*28 bytes are present, the flag is absent.
//
// Both splines share the same count and timestamps so one file stores both.
// ============================================================================

#include "spline.h"
#include "rt/core/camera.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace demo {

    class SplineEditor
    {
    public:
        SplineEditor() = default;

        float m_recordStep = 6.0f;

        // -----------------------------------------------------------------------
        // tryLoad
        //
        // Attempts to load the spline file at m_savePath.
        // If successful, both splines are populated and the camera is immediately
        // snapped to evaluate(0) so the first frame shows the correct position.
        // Call this from Init() so the file is applied before the first render.
        // Returns true if the file was found and loaded correctly.
        // -----------------------------------------------------------------------
        bool tryLoad(Spline<float3>& posSpline,
                     Spline<float3>& targetSpline,
                     rt::core::Camera& cam)
        {
            const bool ok = loadFromFile(posSpline, targetSpline, m_savePath, m_bLoop);
            if (ok)
            {
                posSpline.setLoop(m_bLoop);
                targetSpline.setLoop(m_bLoop);
                snapCameraToStart(posSpline, targetSpline, cam);
                snprintf(m_statusMsg, sizeof(m_statusMsg),
                    "Auto-loaded %d keyframes from %s",
                    posSpline.keyframeCount(), m_savePath);
                m_statusOk = true;
            }
            return ok;
        }

        // -----------------------------------------------------------------------
        // render -- call once per frame from UI().
        // Returns true if any spline data changed.
        // -----------------------------------------------------------------------
        bool render(Spline<float3>& posSpline,
                    Spline<float3>& targetSpline,
                    rt::core::Camera& cam,
                    float splineTime)
        {
            if (!m_bVisible) return false;

            ImGui::SetNextWindowSize(ImVec2(620.0f, 440.0f), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Spline Editor", &m_bVisible))
            {
                ImGui::End();
                return false;
            }

            bool changed = false;

            // ------------------------------------------------------------------
            // Row 1: time display + step + Record
            // ------------------------------------------------------------------
            ImGui::Text("Time: %.2f s", splineTime);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(72.0f);
            ImGui::DragFloat("Step##step", &m_recordStep, 0.5f, 0.5f, 60.0f, "%.1fs");
            ImGui::SameLine();
            if (ImGui::Button("Record"))
            {
                const float nextT = posSpline.isEmpty()
                    ? 0.0f
                    : posSpline.getTime(posSpline.keyframeCount() - 1) + m_recordStep;
                posSpline.addKeyframe(nextT, cam.m_camPos);
                targetSpline.addKeyframe(nextT, cam.m_camTarget);
                changed = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Record current camera pos and target\nat t = lastKeyframe + step");

            // ------------------------------------------------------------------
            // Row 2: Save / Load / Clear / path / Loop toggle
            // ------------------------------------------------------------------
            if (ImGui::Button("Save"))
            {
                const bool ok = saveToFile(posSpline, targetSpline, m_savePath, m_bLoop);
                if (ok)
                    snprintf(m_statusMsg, sizeof(m_statusMsg),
                        "Saved %d keyframes to %s",
                        posSpline.keyframeCount(), m_savePath);
                else
                    snprintf(m_statusMsg, sizeof(m_statusMsg),
                        "Save FAILED: %s", m_savePath);
                m_statusOk = ok;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Save to: %s", m_savePath);

            ImGui::SameLine();
            if (ImGui::Button("Load"))
            {
                const bool ok = loadFromFile(posSpline, targetSpline, m_savePath, m_bLoop);
                if (ok)
                {
                    posSpline.setLoop(m_bLoop);
                    targetSpline.setLoop(m_bLoop);
                    snapCameraToStart(posSpline, targetSpline, cam);
                    snprintf(m_statusMsg, sizeof(m_statusMsg),
                        "Loaded %d keyframes from %s",
                        posSpline.keyframeCount(), m_savePath);
                }
                else
                {
                    snprintf(m_statusMsg, sizeof(m_statusMsg),
                        "Load FAILED: %s", m_savePath);
                }
                m_statusOk = ok;
                if (ok) changed = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Load from: %s", m_savePath);

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button("Clear All"))
            {
                posSpline.clear();
                targetSpline.clear();
                snprintf(m_statusMsg, sizeof(m_statusMsg), "Cleared.");
                m_statusOk = true;
                changed = true;
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::SetNextItemWidth(180.0f);
            ImGui::InputText("##path", m_savePath, sizeof(m_savePath));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("File path for Save / Load");

            if (m_statusMsg[0] != '\0')
            {
                ImGui::SameLine();
                ImGui::TextColored(
                    m_statusOk ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
                               : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                    "%s", m_statusMsg);
            }

            // ------------------------------------------------------------------
            // Row 3: Loop toggle
            // ------------------------------------------------------------------
            if (ImGui::Checkbox("Loop spline", &m_bLoop))
            {
                posSpline.setLoop(m_bLoop);
                targetSpline.setLoop(m_bLoop);
                changed = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "When enabled, the spline loops back to the\n"
                    "first keyframe after reaching the last one.\n"
                    "This flag is saved into the binary file.");

            ImGui::Separator();

            // ------------------------------------------------------------------
            // Keyframe table
            // ------------------------------------------------------------------
            const int count = posSpline.keyframeCount();

            if (count == 0)
            {
                ImGui::TextDisabled("No keyframes.  Enter free-cam (Tab) and press Record.");
                ImGui::End();
                return changed;
            }

            bool sortNeeded = false;

            constexpr ImGuiTableFlags tableFlags =
                ImGuiTableFlags_Borders         |
                ImGuiTableFlags_RowBg           |
                ImGuiTableFlags_ScrollY         |
                ImGuiTableFlags_SizingFixedFit;

            if (ImGui::BeginTable("##kf", 5, tableFlags, ImVec2(0.0f, 280.0f)))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("#",                   ImGuiTableColumnFlags_WidthFixed,   24.0f);
                ImGui::TableSetupColumn("Time",                ImGuiTableColumnFlags_WidthFixed,   60.0f);
                ImGui::TableSetupColumn("Position (x  y  z)",  ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Target (x  y  z)",    ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Del",                 ImGuiTableColumnFlags_WidthFixed,   28.0f);
                ImGui::TableHeadersRow();

                int deleteIdx = -1;

                for (int i = 0; i < count; i++)
                {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", i);

                    ImGui::TableSetColumnIndex(1);
                    float t = posSpline.getTime(i);
                    ImGui::SetNextItemWidth(-1.0f);
                    char tlbl[32]; snprintf(tlbl, sizeof(tlbl), "##t%d", i);
                    if (ImGui::DragFloat(tlbl, &t, 0.001f, 0.0f, 9999.0f, "%.3f"))
                    {
                        posSpline.setTime(i, t);
                        targetSpline.setTime(i, t);
                        changed = true;
                    }
                    // Only sort once the edit is fully committed (Enter / Tab /
                    // click away).  DragFloat returns true on every keystroke
                    // during Ctrl+click typing, so we must not sort mid-input
                    // or the row indices scramble and ImGui snaps the value.
                    if (ImGui::IsItemDeactivatedAfterEdit())
                        sortNeeded = true;

                    ImGui::TableSetColumnIndex(2);
                    float3 pos   = posSpline.getValue(i);
                    float  pv[3] = { pos.x, pos.y, pos.z };
                    ImGui::SetNextItemWidth(-1.0f);
                    char plbl[32]; snprintf(plbl, sizeof(plbl), "##p%d", i);
                    if (ImGui::DragFloat3(plbl, pv, 0.001f, -4.0f, 4.0f, "%.3f"))
                    {
                        posSpline.setValue(i, float3(pv[0], pv[1], pv[2]));
                        changed = true;
                    }

                    ImGui::TableSetColumnIndex(3);
                    float3 tgt   = targetSpline.getValue(i);
                    float  tv[3] = { tgt.x, tgt.y, tgt.z };
                    ImGui::SetNextItemWidth(-1.0f);
                    char glbl[32]; snprintf(glbl, sizeof(glbl), "##g%d", i);
                    if (ImGui::DragFloat3(glbl, tv, 0.001f, -4.0f, 4.0f, "%.3f"))
                    {
                        targetSpline.setValue(i, float3(tv[0], tv[1], tv[2]));
                        changed = true;
                    }

                    ImGui::TableSetColumnIndex(4);
                    char dlbl[32]; snprintf(dlbl, sizeof(dlbl), "X##%d", i);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
                    if (ImGui::SmallButton(dlbl))
                        deleteIdx = i;
                    ImGui::PopStyleColor();
                }

                ImGui::EndTable();

                if (deleteIdx >= 0)
                {
                    posSpline.removeKeyframe(deleteIdx);
                    targetSpline.removeKeyframe(deleteIdx);
                    changed = true;
                }
            }

            // Sort only when an edit was fully committed this frame.
            if (sortNeeded)
            {
                posSpline.sortByTime();
                targetSpline.sortByTime();
            }

            ImGui::Separator();
            if (count < 4)
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.1f, 1.0f),
                    "Need >= 4 keyframes to run the spline  (%d so far).", count);
            else
            {
                if (m_bLoop)
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                        "%d keyframes  |  %.1f s  to  %.1f s  |  loop cycle: %.1f s",
                        count, posSpline.startTime(), posSpline.endTime(),
                        posSpline.loopDuration());
                else
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                        "%d keyframes  |  %.1f s  to  %.1f s",
                        count, posSpline.startTime(), posSpline.endTime());
            }

            ImGui::End();
            return changed;
        }

        void show()   { m_bVisible = true;        }
        void hide()   { m_bVisible = false;       }
        void toggle() { m_bVisible = !m_bVisible; }
        [[nodiscard]] bool isVisible() const { return m_bVisible; }
        [[nodiscard]] bool isLooping() const { return m_bLoop;    }

    private:
        bool m_bVisible       = false;
        char m_savePath[256]  = "assets/camera_spline.bin";
        char m_statusMsg[128] = {};
        bool m_statusOk       = true;
        bool m_bLoop          = false;  // appended last to preserve layout

        // -----------------------------------------------------------------------
        // snapCameraToStart
        //
        // Sets the camera position and target to the spline values at t=0
        // (the first keyframe's recorded position) and rebuilds the matrix.
        // Called after every successful load so the camera is in the right
        // place before the first rendered frame.
        // -----------------------------------------------------------------------
        static void snapCameraToStart(const Spline<float3>& posSpline,
                                      const Spline<float3>& targetSpline,
                                      rt::core::Camera& cam)
        {
            if (posSpline.keyframeCount() < 1) return;
            const float startT      = posSpline.startTime();
            cam.m_camPos    = posSpline.getValue(0);
            cam.m_camTarget = targetSpline.getValue(0);
            (void)startT;
            cam.updateCameraToWorld();
        }

        // -----------------------------------------------------------------------
        // saveToFile
        //
        // Binary layout:
        //   [4 bytes]  uint32_t   N
        //   [N * 28]   keyframes  (time, pos.xyz, target.xyz)
        //   [1 byte]   uint8_t    loop flag
        // -----------------------------------------------------------------------
        static bool saveToFile(const Spline<float3>& posSpline,
                               const Spline<float3>& targetSpline,
                               const char* path,
                               const bool bLoop)
        {
            // Remove the old file first so there is no stale data left over
            // if the new file is shorter than the previous one.
            // "wb" truncates on open, but an explicit remove guarantees the
            // filesystem has no cached remnants of the old content.
            ::remove(path);

            FILE* f = fopen(path, "wb");
            if (!f) return false;

            const uint32_t n = static_cast<uint32_t>(posSpline.keyframeCount());
            fwrite(&n, sizeof(uint32_t), 1, f);

            for (uint32_t i = 0; i < n; i++)
            {
                float data[7];
                data[0] = posSpline.getTime(static_cast<int>(i));
                const float3 pos = posSpline.getValue(static_cast<int>(i));
                data[1] = pos.x;
                data[2] = pos.y;
                data[3] = pos.z;
                const float3 tgt = targetSpline.getValue(static_cast<int>(i));
                data[4] = tgt.x;
                data[5] = tgt.y;
                data[6] = tgt.z;
                fwrite(data, sizeof(float), 7, f);
            }

            // Append loop flag at end (backward-compatible extension)
            const uint8_t loopByte = bLoop ? 1 : 0;
            fwrite(&loopByte, sizeof(uint8_t), 1, f);

            // Flush the C runtime buffer to the OS before closing.
            fflush(f);
            fclose(f);
            return true;
        }

        // -----------------------------------------------------------------------
        // loadFromFile
        //
        // Reads the binary format.  The loop flag byte at the end is optional
        // for backward compatibility: if the file has exactly 4 + N*28 bytes,
        // the flag is absent and defaults to false.
        // -----------------------------------------------------------------------
        static bool loadFromFile(Spline<float3>& posSpline,
                                 Spline<float3>& targetSpline,
                                 const char* path,
                                 bool& outLoop)
        {
            FILE* f = fopen(path, "rb");
            if (!f) return false;

            uint32_t n = 0;
            if (fread(&n, sizeof(uint32_t), 1, f) != 1)
            {
                fclose(f);
                return false;
            }

            if (n > 1024)
            {
                fclose(f);
                return false;
            }

            posSpline.clear();
            targetSpline.clear();

            for (uint32_t i = 0; i < n; i++)
            {
                float data[7];
                if (fread(data, sizeof(float), 7, f) != 7)
                {
                    posSpline.clear();
                    targetSpline.clear();
                    fclose(f);
                    return false;
                }

                posSpline.addKeyframe(data[0],
                    float3(data[1], data[2], data[3]));
                targetSpline.addKeyframe(data[0],
                    float3(data[4], data[5], data[6]));
            }

            // Sort both splines by time after loading.
            // Keyframes may have been saved or edited out of order -- sorting
            // here ensures endTime() and findSegment() work correctly.
            posSpline.sortByTime();
            targetSpline.sortByTime();

            // Try to read the loop flag (appended at end).
            // If absent (old file format), default to false.
            uint8_t loopByte = 0;
            if (fread(&loopByte, sizeof(uint8_t), 1, f) == 1)
                outLoop = (loopByte != 0);
            else
                outLoop = false;

            fclose(f);
            return true;
        }
    };

}  // namespace demo