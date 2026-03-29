#pragma once

#include "rt/scene/scene.h"

#include "rt/lights/directional_light.h"
#include "rt/lights/point_light.h"
#include "rt/lights/spotlight.h"

namespace rt::management {

    class LightManager 
    {
    public:
        LightManager() = default;
        ~LightManager() = default;

        uint addDirectionalLight(const rt::lights::DirectionalLight& light);
        uint addPointLight(const rt::lights::PointLight& light);
        uint addSpotLight(const rt::lights::SpotLight& light);

        void removeDirectionalLight(uint idx);
        void removePointLight(uint idx);
        void removeSpotLight(uint idx);

        std::vector<rt::lights::DirectionalLight>& getDirectionalLights() { return m_dirLights; }
        std::vector<rt::lights::PointLight>& getPointLights() { return m_ptLights; }
        std::vector<rt::lights::SpotLight>& getSpotLights() { return m_spotLights; }

        [[nodiscard]] float3 calculateLighting(const float3& I, const float3& N, const float3& V,
                                              const rendering::Material& material,
                                              const scene::Scene& scene,
                                              const rendering::MaterialManager& matMgr,
                                              bool accumulate = true) const;

        [[nodiscard]] uint directionalCount() const { return static_cast<uint>(m_dirLights.size()); }
        [[nodiscard]] uint pointCount() const { return static_cast<uint>(m_ptLights.size()); }
        [[nodiscard]] uint spotCount() const { return static_cast<uint>(m_spotLights.size()); }
        [[nodiscard]] uint totalCount() const { return directionalCount() + pointCount() + spotCount(); }

    private:
        std::vector<rt::lights::DirectionalLight> m_dirLights;
        std::vector<rt::lights::PointLight> m_ptLights;
        std::vector<rt::lights::SpotLight> m_spotLights;
    };

}  // namespace Tmpl8::Management