#pragma once

#include "tmpl8/template.h"
#include <algorithm>
#include <cmath>

namespace demo {

    // ============================================================================
    // Spline<T>
    //
    // Generic non-uniform Catmull-Rom spline.
    //
    // Endpoint handling (non-loop mode):
    //   Standard Catmull-Rom requires a phantom point before the first and after
    //   the last keyframe, which meant the first and last recorded keyframes were
    //   never actually interpolated.  This implementation duplicates the endpoints
    //   internally inside evaluate() so every recorded keyframe is reachable and
    //   the minimum usable count is 2 (not 4).
    //
    // Loop mode:
    //   When m_bLoop is true, control-point indices wrap around using modular
    //   arithmetic so the curve returns smoothly from the last keyframe back to
    //   the first.  An extra virtual segment of average-step duration bridges
    //   the last keyframe to the first in the time domain.
    //
    //   Reference for the wrapping technique:
    //     Habrador, "Catmull-Rom Splines" tutorial
    //     https://www.habrador.com/tutorials/interpolation/1-catmull-rom-splines/
    //     (ClampListPos modular-index pattern for closed loops)
    //
    //     Boost.Math Catmull-Rom documentation:
    //     https://www.boost.org/doc/libs/master/libs/math/doc/html/math_toolkit/catmull_rom.html
    //     (Internally represents curves as closed via index wrapping)
    //
    // Reference:
    //   Catmull, E. & Rom, R. (1974). "A Class of Local Interpolating Splines."
    //   In R. Barnhill & R. Riesenfeld (Eds.), Computer Aided Geometric Design
    //   (pp. 317-326). Academic Press.
    // ============================================================================
    template<typename T>
    class Spline
    {
    public:
        Spline()  = default;
        ~Spline() = default;

        void addKeyframe(const float t, const T& v)
        {
            m_times.push_back(t);
            m_values.push_back(v);
        }

        // Sort all keyframes by time in ascending order.
        // Call this after loading from file or after editing times in the UI
        // to guarantee findSegment() and endTime() work correctly.
        void sortByTime()
        {
            const int n = static_cast<int>(m_times.size());
            if (n < 2) return;

            // Build an index array, sort it by time, then reorder both vectors.
            std::vector<int> idx(n);
            for (int i = 0; i < n; i++) idx[i] = i;
            std::sort(idx.begin(), idx.end(), [this](const int a, const int b) {
                return m_times[a] < m_times[b];
            });

            std::vector<float> sortedTimes(n);
            std::vector<T>     sortedValues(n);
            for (int i = 0; i < n; i++) {
                sortedTimes[i]  = m_times[idx[i]];
                sortedValues[i] = m_values[idx[i]];
            }
            m_times  = std::move(sortedTimes);
            m_values = std::move(sortedValues);
        }

        void removeKeyframe(const int i)
        {
            const int n = static_cast<int>(m_times.size());
            if (i < 0 || i >= n) return;
            m_times.erase(m_times.begin()   + i);
            m_values.erase(m_values.begin() + i);
        }

        void setTime(const int i, const float t)
        {
            const int n = static_cast<int>(m_times.size());
            if (i < 0 || i >= n) return;
            m_times[i] = t;
            // Do NOT sort here -- sorting mid-drag scrambles row indices and
            // causes ImGui to lose the active widget, snapping the value.
            // Call sortByTime() explicitly after editing is complete.
        }

        void setValue(const int i, const T& v)
        {
            const int n = static_cast<int>(m_values.size());
            if (i < 0 || i >= n) return;
            m_values[i] = v;
        }

        void clear()
        {
            m_times.clear();
            m_values.clear();
        }

        void setLoop(const bool loop) { m_bLoop = loop; }
        [[nodiscard]] bool isLooping() const { return m_bLoop; }

        // Total duration of one loop cycle.
        // When looping, an extra segment connects the last keyframe back
        // to the first.  Its duration equals the average segment length,
        // keeping the return speed consistent with the rest of the path.
        [[nodiscard]] float loopDuration() const
        {
            const int n = static_cast<int>(m_times.size());
            if (n < 2) return 0.0f;
            const float totalSpan = m_times.back() - m_times.front();
            if (!m_bLoop) return totalSpan;
            const float avgStep = totalSpan / static_cast<float>(n - 1);
            return totalSpan + avgStep;
        }

        // -----------------------------------------------------------------------
        // evaluate
        //
        // Returns the interpolated value at globalT.
        //
        // NON-LOOP MODE:
        //   Clamps outside [startTime, endTime] to the first/last keyframe value.
        //   Endpoint duplication for phantom points.
        //
        // LOOP MODE:
        //   Wraps globalT into [startTime, startTime + loopDuration) via fmod.
        //   Control-point indices wrap modularly (standard closed-loop technique).
        //
        // Formula (Catmull-Rom expanded matrix form):
        //   q(t) = 0.5 * (
        //       2*p1
        //     + (-p0 + p2)                 * t
        //     + (2*p0 - 5*p1 + 4*p2 - p3) * t^2
        //     + (-p0 + 3*p1 - 3*p2 + p3)  * t^3
        //   )
        //   where t in [0,1]
        //
        // Reference:
        //   Lengyel, E. (2011). Mathematics for 3D Game Programming and
        //   Computer Graphics (3rd ed.), section 12.2, p. 441.
        // -----------------------------------------------------------------------
        T evaluate(const float globalT) const
        {
            const int n = static_cast<int>(m_times.size());
            if (n == 0) return T{};
            if (n == 1) return m_values[0];

            // ==============================================================
            // LOOP PATH
            // ==============================================================
            if (m_bLoop && n >= 2)
            {
                const float totalSpan = m_times.back() - m_times.front();
                const float avgStep   = totalSpan / static_cast<float>(n - 1);
                const float cycleDur  = totalSpan + avgStep; // includes return segment
                const float baseT     = m_times.front();

                // Wrap globalT into [0, cycleDur)
                float wrapped = globalT - baseT;
                wrapped = std::fmod(wrapped, cycleDur);
                if (wrapped < 0.0f) wrapped += cycleDur;

                // Find which segment we're in.
                // Segments 0..n-2 are the normal keyframe-to-keyframe segments.
                // Segment n-1 is the virtual return segment (last -> first).
                int   seg      = n - 1;
                float segStart = m_times.back() - baseT;
                float segEnd   = cycleDur;

                for (int s = 0; s < n - 1; s++)
                {
                    const float sEnd = m_times[s + 1] - baseT;
                    if (wrapped < sEnd)
                    {
                        seg      = s;
                        segStart = m_times[s] - baseT;
                        segEnd   = sEnd;
                        break;
                    }
                }

                const float segLen = segEnd - segStart;
                float localT = (segLen > 0.0f)
                    ? (wrapped - segStart) / segLen
                    : 0.0f;
                if (localT < 0.0f) localT = 0.0f;
                if (localT > 1.0f) localT = 1.0f;

                // Modular index wrapping -- the standard closed-loop Catmull-Rom
                // technique.  Each index wraps around using ((idx % n) + n) % n.
                //
                // Reference:
                //   Habrador tutorial "ClampListPos" pattern
                //   Boost.Math catmull_rom internal closed representation
                const int i1 =   seg % n;
                const int i0 = ((seg - 1) % n + n) % n;
                const int i2 =  (seg + 1) % n;
                const int i3 =  (seg + 2) % n;

                const T& p0 = m_values[i0];
                const T& p1 = m_values[i1];
                const T& p2 = m_values[i2];
                const T& p3 = m_values[i3];

                const float t2 = localT * localT;
                const float t3 = t2 * localT;

                return (p1 * 2.0f
                    + (p2 - p0) * localT
                    + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2
                    + (p0 * -1.0f + p1 * 3.0f - p2 * 3.0f + p3) * t3)
                    * 0.5f;
            }

            // ==============================================================
            // ORIGINAL NON-LOOP PATH (unchanged)
            // ==============================================================

            // Clamp to valid range
            if (globalT <= m_times[0])     return m_values[0];
            if (globalT >= m_times[n - 1]) return m_values[n - 1];

            // Find segment i such that times[i] <= globalT < times[i+1]
            const int i = findSegment(globalT);

            const float segStart = m_times[i];
            const float segEnd   = m_times[i + 1];
            const float segLen   = segEnd - segStart;

            float localT = (segLen > 0.0f)
                ? (globalT - segStart) / segLen
                : 0.0f;
            // Safety clamp -- should not be needed after the range check above
            // but guards against floating point edge cases.
            if (localT < 0.0f) localT = 0.0f;
            if (localT > 1.0f) localT = 1.0f;

            // Endpoint duplication: if i-1 or i+2 is out of range, mirror the
            // nearest valid endpoint rather than reading garbage.
            const T& p1 = m_values[i];
            const T& p2 = m_values[i + 1];
            const T& p0 = (i > 0)     ? m_values[i - 1] : p1;   // duplicate first
            const T& p3 = (i + 2 < n) ? m_values[i + 2] : p2;   // duplicate last

            const float t2 = localT * localT;
            const float t3 = t2 * localT;

            return (p1 * 2.0f
                + (p2 - p0) * localT
                + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2
                + (p0 * -1.0f + p1 * 3.0f - p2 * 3.0f + p3) * t3)
                * 0.5f;
        }

        [[nodiscard]] bool  isEmpty()       const { return m_times.empty(); }
        [[nodiscard]] int   keyframeCount() const { return static_cast<int>(m_times.size()); }
        [[nodiscard]] float startTime()     const { return m_times.empty() ? 0.0f : m_times.front(); }
        [[nodiscard]] float endTime()       const { return m_times.empty() ? 0.0f : m_times.back();  }
        [[nodiscard]] float getTime(int i)  const { return m_times[i];  }
        [[nodiscard]] T     getValue(int i) const { return m_values[i]; }

    private:
        // Returns segment index i such that times[i] <= globalT < times[i+1].
        // Range: [0, n-2].
        [[nodiscard]] int findSegment(const float globalT) const
        {
            const int n = static_cast<int>(m_times.size());
            // Linear scan -- fast for typical camera paths (< 64 keyframes).
            int i = 0;
            while (i < n - 2 && m_times[i + 1] <= globalT)
                ++i;
            return i;
        }

        std::vector<float> m_times;
        std::vector<T>     m_values;
        bool               m_bLoop = false;
    };

}  // namespace demo