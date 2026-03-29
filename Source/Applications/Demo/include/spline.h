#pragma once

#include "tmpl8/template.h"
#include <algorithm>

namespace demo {

    // ============================================================================
    // Spline<T>
    //
    // Generic non-uniform Catmull-Rom spline.
    //
    // Endpoint handling:
    //   Standard Catmull-Rom requires a phantom point before the first and after
    //   the last keyframe, which meant the first and last recorded keyframes were
    //   never actually interpolated.  This implementation duplicates the endpoints
    //   internally inside evaluate() so every recorded keyframe is reachable and
    //   the minimum usable count is 2 (not 4).
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

        // -----------------------------------------------------------------------
        // evaluate
        //
        // Returns the interpolated value at globalT.
        // Clamps outside [startTime, endTime] to the first/last keyframe value.
        //
        // Endpoint duplication: when the segment is at the first or last index,
        // the missing neighbour is replaced with the endpoint value itself.
        // This means p0==p1 at the start and p2==p3 at the end, which causes
        // the curve to arrive/depart tangentially from the endpoint -- a natural
        // and well-defined behaviour.
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
    };

}  // namespace demo