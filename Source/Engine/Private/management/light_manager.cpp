#include "rt/management/light_manager.h"
#include "rt/lights/light_sample.h"
#include "tmpl8/template.h"

namespace rt::management {
    uint LightManager::addDirectionalLight(const rt::lights::DirectionalLight &light)
    {
        m_dirLights.push_back(light);
        return static_cast<uint>(m_dirLights.size() - 1);
    }

    uint LightManager::addPointLight(const rt::lights::PointLight &light)
    {
        m_ptLights.push_back(light);
        return static_cast<uint>(m_ptLights.size() - 1);
    }

    uint LightManager::addSpotLight(const rt::lights::SpotLight &light)
    {
        m_spotLights.push_back(light);
        return static_cast<uint>(m_spotLights.size() - 1);
    }

    void LightManager::removeDirectionalLight(const uint idx)
    {
        if (idx < static_cast<uint>(m_dirLights.size()))
            m_dirLights.erase(m_dirLights.begin() + idx);
    }

    void LightManager::removePointLight(const uint idx)
    {
        if (idx < static_cast<uint>(m_ptLights.size()))
            m_ptLights.erase(m_ptLights.begin() + idx);
    }

    void LightManager::removeSpotLight(const uint idx)
    {
        if (idx < static_cast<uint>(m_spotLights.size()))
            m_spotLights.erase(m_spotLights.begin() + idx);
    }

    float3 LightManager::calculateLighting(const float3 &I, const float3 &N, const float3 &V, const rendering::Material &material,
                                           const scene::Scene &scene, const rendering::MaterialManager& matMgr,
                                           const bool accumulate) const
    {
        const uint nDir  = static_cast<uint>(m_dirLights.size());
        const uint nPt   = static_cast<uint>(m_ptLights.size());
        const uint nSpot = static_cast<uint>(m_spotLights.size());
        const uint total = nDir + nPt + nSpot;

        if (total == 0) return {0};

        // When accumulating: sample one light, scale by total (1/pdf = total).
        // Variance is averaged out over frames by the accumulator.
        // When not accumulating (camera moving): sample all lights for a stable image.
        if (accumulate)
        {
            const uint pick = min(
                static_cast<uint>(RandomFloat() * static_cast<float>(total)),
                total - 1u);

            lights::LightSample s{};
            if (pick < nDir)
                s = m_dirLights[pick].sample(I);
            else if (pick < nDir + nPt)
                s = m_ptLights[pick - nDir].sample(I);
            else
                s = m_spotLights[pick - nDir - nPt].sample(I);

            return lights::evaluate(s, I, N, V, material, scene, matMgr)
                   * static_cast<float>(total);
        }

        // Full loop — no noise when not accumulating
        float3 result{0};
        for (const auto& l : m_dirLights)  { result += lights::evaluate(l.sample(I),  I, N, V, material, scene, matMgr); }
        for (const auto& l : m_ptLights)   { result += lights::evaluate(l.sample(I),  I, N, V, material, scene, matMgr); }
        for (const auto& l : m_spotLights) { result += lights::evaluate(l.sample(I),  I, N, V, material, scene, matMgr); }
        return result;
    }

}  // namespace Tmpl8::Management