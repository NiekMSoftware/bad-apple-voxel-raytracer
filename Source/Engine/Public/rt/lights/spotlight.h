#pragma once

#include "tmpl8/tmpl8math.h"
#include "tmpl8/template.h"
#include "rt/scene/scene.h"
#include "rt/rendering/material.h"

namespace rt::lights {

    class SpotLight 
    {
    public:
        explicit SpotLight(
            const float3 pos = float3(0),
            const float3 dir = float3(0, -1, 0),
            const float3 col = float3(1),
            const float angle = 45.0f,
            const float softBand = 0.2f
        )
            : m_position(pos), m_direction(normalize(dir)), m_color(col),
              m_spotAngle(angle), m_softBandRatio(clamp(softBand, 0.0f, 1.0f))
        {
            updateConeValues();
        }

        ~SpotLight() = default;

        // getters
        [[nodiscard]] float3 getDirection() const { return m_direction; }
        [[nodiscard]] float3 getPosition() const { return m_position; }
        [[nodiscard]] float3 getColor() const { return m_color; }
        [[nodiscard]] float getSpotAngle() const { return m_spotAngle; }
        [[nodiscard]] float getIntensity() const { return m_intensity; }
        [[nodiscard]] float getSoftBandRatio() const { return m_softBandRatio; }
        [[nodiscard]] float getRadius() const { return m_radius; }
        [[nodiscard]] float getPitch() const { return m_pitch; }
        [[nodiscard]] float getYaw() const { return m_yaw; }

        // setters
        void setPosition(const float3& pos) { m_position = pos; }
        void setColor(const float3& col) { m_color = col; }
        void setIntensity(const float power) { m_intensity = power; }
        void setRadius(const float r) { m_radius = r; }

        void setDirection(const float3& dir)
        {
            m_direction = normalize(dir);
        }

        void setSpotAngle(const float angle)
        {
            m_spotAngle = clamp(angle, 0.0f, 180.0f);
            updateConeValues();
        }

        void setSoftBandRatio(const float ratio)
        {
            m_softBandRatio = clamp(ratio, 0.0f, 1.0f);
            updateConeValues();
        }

        void setPitch(const float p)
        {
            m_pitch = clamp(p, -90.0f, 90.0f);
            updateDirectionFromPitchYaw();
        }

        void setYaw(const float y)
        {
            m_yaw = y;
            updateDirectionFromPitchYaw();
        }

        // Returns a random point within a sphere around the spotlight
        // position, modeling the light as a small area light.
        [[nodiscard]] float3 getRandomPoint() const
        {
            if (m_radius <= 0.0f) return m_position;
            float3 offset{};
            do {
                offset = float3(
                    RandomFloat() * 2 - 1,
                    RandomFloat() * 2 - 1,
                    RandomFloat() * 2 - 1
                );
            } while (dot(offset, offset) > 1.0f);
            return m_position + m_radius * offset;
        }

        LightSample sample(const float3& surfacePoint, const bool accumulate = true) const
        {
            const float3 lightPos = accumulate ? getRandomPoint() : m_position;

            const float3 lightToPoint = normalize(surfacePoint - lightPos);
            const float alignment = dot(lightToPoint, m_direction);

            // Outside outer cone - zero radiance
            if (alignment <= m_cosOuterCone)
                return { float3(0), float3(0), 0.0f, false };

            float coneFalloff = 1.0f;
            if (alignment < m_cosInnerCone) {
                const float t = (alignment - m_cosOuterCone) / (m_cosInnerCone - m_cosOuterCone);
                coneFalloff = t;
            }

            float3 toLight = lightPos - surfacePoint;
            const float distance = length(toLight);
            toLight = normalize(toLight);

            const float attenuation = coneFalloff / (distance * distance);

            return { toLight, m_color * m_intensity * attenuation, distance, true };
        }

    private:
        float3 m_position  = float3(0);
        float3 m_direction = float3(0, -1, 0);  // where the spotlight AIMS

        float3 m_color     = float3(1);
        float m_intensity  = 1.0f;

        float m_spotAngle     = 45.0f;   // half-angle of the cone in degrees
        float m_softBandRatio = 0.2f;    // fraction of the cone that is soft
        float m_radius        = 0.05f;   // sphere radius for area light jitter

        float m_pitch = 0.0f;
        float m_yaw   = 0.0f;

        float m_cosInnerCone = 0.0f;
        float m_cosOuterCone = 0.0f;

        // cosOuterCone = cos(spotAngle)
        // cosInnerCone = cos(spotAngle - softBand)
        // Since the inner angle is smaller, cosInnerCone > cosOuterCone.
        // Points with alignment >= cosInnerCone are fully lit.
        // Points with alignment <= cosOuterCone get no light.
        // Points between get a linear falloff.
        void updateConeValues()
        {
            const float outerRad = m_spotAngle * (PI / 180.0f);
            const float softBand = outerRad * m_softBandRatio;
            const float innerRad = outerRad - softBand;
            m_cosInnerCone = cosf(innerRad);
            m_cosOuterCone = cosf(outerRad);
        }

        void updateDirectionFromPitchYaw()
        {
            const float pitchRad = m_pitch * (PI / 180.0f);
            const float yawRad   = m_yaw * (PI / 180.0f);
            m_direction = normalize(float3(
                cosf(pitchRad) * sinf(yawRad),
                sinf(pitchRad),
                cosf(pitchRad) * cosf(yawRad)
            ));
        }
    };

}  // Tmpl8