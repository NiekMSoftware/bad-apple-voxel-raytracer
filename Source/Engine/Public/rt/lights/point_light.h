#pragma once

#include "light_sample.h"

namespace rt::lights {

    class PointLight
    {
    public:
        explicit PointLight(const float3 pos = float3(1), const float3 col = float3(1))
            : m_position(pos), m_color(col) {}

        void setPosition(const float3& pos) { m_position = pos; }
        void setColor(const float3& col) { m_color = col; }
        void setIntensity(const float power) { m_intensity = power; }
        void setRadius(const float r) { m_radius = r; }

        [[nodiscard]] float3 getPosition() const { return m_position; }
        [[nodiscard]] float3 getColor() const { return m_color; }
        [[nodiscard]] float getIntensity() const { return m_intensity; }
        [[nodiscard]] float getRadius() const { return m_radius; }

        [[nodiscard]] float3 getRandomPoint() const
        {
            if (m_radius <= 0.0f) return m_position;
            float3 offset{};
            do {
                offset = float3(RandomFloat() * 2 - 1, RandomFloat() * 2 - 1, RandomFloat() * 2 - 1);
            } while (dot(offset, offset) > 1.0f);
            return m_position + m_radius * offset;
        }

        LightSample sample(const float3& surfacePoint, const bool accumulate = true) const
        {
            const float3 lightPos = accumulate ? getRandomPoint() : m_position;
            float3 toLight = lightPos - surfacePoint;
            const float distance = length(toLight);
            if (distance < 1e-6f)
                return { float3(0), float3(0), 0.0f, false };
            toLight = normalize(toLight);

            const float attenuation = 1.0f / (distance * distance);

            return { toLight, m_color * m_intensity * attenuation, distance, true };
        }

    private:
        float3 m_position{1};
        float3 m_color{1};
        float m_intensity{1};
        float m_radius{0.05f};
    };

}  // Tmpl8