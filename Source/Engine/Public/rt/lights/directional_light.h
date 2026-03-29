#pragma once

#include "light_sample.h"

namespace rt::lights {

    class DirectionalLight 
    {
    public:

        DirectionalLight() = default;
        explicit DirectionalLight(const float3 dir)
            : m_direction(normalize(dir)) {}

        void setColor(const float3 col) { m_color = col; }
        void setIntensity(const float power) { m_intensity = power; }
        void setDirection(const float3 dir) { m_direction = dir; }
        void setAngularRadius(const float r) { m_angularRadius = r; }

        [[nodiscard]] float3 getColor() const { return m_color; }
        [[nodiscard]] float getIntensity() const { return m_intensity; }
        [[nodiscard]] float3 getDirection() const { return m_direction; }
        [[nodiscard]] float getAngularRadius() const { return m_angularRadius; }

        [[nodiscard]] float3 getRandomPoint() const
        {
            const float3 dir = normalize(m_direction);
            if (m_angularRadius <= 0.0f) return dir * 1000.0f;
            const float3 tmp = (fabs(dir.x) > 0.99f) ? float3(0, 1, 0) : float3(1, 0, 0);
            const float3 t = normalize(cross(dir, tmp));
            const float3 b = cross(dir, t);
            const float r1 = RandomFloat() - 0.5f;
            const float r2 = RandomFloat() - 0.5f;
            return normalize(dir + m_angularRadius * (t * r1 + b * r2)) * 1000.0f;
        }

        LightSample sample(const float3& surfacePoint, const bool accumulate = true) const
        {
            const float3 point = accumulate
                ? getRandomPoint()
                : normalize(m_direction) * 1000.0f;

            float3 toLight = point - surfacePoint;
            toLight = normalize(toLight);

            return { toLight, m_color * m_intensity, LARGE_FLOAT, true };
        }

    private:
        float3 m_direction{-1, 0.5f, 1};
        float3 m_color{1};
        float m_intensity{1};
        float m_angularRadius{0.1f};
    };

}  // Tmpl8