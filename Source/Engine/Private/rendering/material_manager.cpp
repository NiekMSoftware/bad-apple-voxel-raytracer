#include "rt/rendering/material_manager.h"

namespace rt::rendering {

    MaterialManager::MaterialManager()
    {
        // Initialize all slots to unused
        memset(m_aMaterialsInUse, 0, sizeof(m_aMaterialsInUse));

        // Initialize all slots with the default material
        for (uint i = 0; i < kMaxMaterials; i++) {
            m_aMaterials[i] = createDefaultMaterial();
            m_aMaterials[i].m_id = i;
        }
    }

    void MaterialManager::initialize()
    {
        memset(m_aMaterialsInUse, 0, sizeof(m_aMaterialsInUse));
        m_materialCount = 0;
        m_selectedMaterialIndex = 0;

        // Material 0: Default Diffuse
        Material defaultMat = createDefaultMaterial();
        defaultMat.m_baseColor = float3(0.8f, 0.8f, 0.8f);
        defaultMat.m_metallic = 0.0f;
        defaultMat.m_roughness = 0.5f;
        defaultMat.m_anisotropy = 0.0f;
        defaultMat.m_transparency = 0.0f;
        defaultMat.m_indexOfRefraction = 1.5f;
        registerMaterial(defaultMat);

        // Material 1: Mirror
        Material mirror = createDefaultMaterial();
        mirror.m_baseColor = float3(0.95f, 0.95f, 0.95f);
        mirror.m_metallic = 1.0f;
        mirror.m_roughness = 0.0f;
        mirror.m_anisotropy = 0.0f;
        mirror.m_indexOfRefraction = 1.5f;
        mirror.m_transparency = 0.0f;
        registerMaterial(mirror);

        // Material 2: Glass
        Material glass = createDefaultMaterial();
        glass.m_baseColor = float3(1.0f, 1.0f, 1.0f);
        glass.m_metallic = 0.0f;
        glass.m_roughness = 0.0f;
        glass.m_anisotropy = 0.0f;
        glass.m_indexOfRefraction = 1.5f;
        glass.m_transparency = 1.0f;
        glass.m_absorptionDensity = 1.0f;
        registerMaterial(glass);

        // Material 3: Rough Plastic
        Material roughPlastic = createDefaultMaterial();
        roughPlastic.m_baseColor = float3(0.8f, 0.2f, 0.2f);
        roughPlastic.m_metallic = 0.0f;
        roughPlastic.m_roughness = 0.8f;
        roughPlastic.m_anisotropy = 0.0f;
        roughPlastic.m_indexOfRefraction = 1.46f;
        roughPlastic.m_transparency = 0.0f;
        registerMaterial(roughPlastic);

        // Material 4: Glossy Plastic
        Material glossyPlastic = createDefaultMaterial();
        glossyPlastic.m_baseColor = float3(0.1f, 0.3f, 0.8f);
        glossyPlastic.m_metallic = 0.0f;
        glossyPlastic.m_roughness = 0.2f;
        glossyPlastic.m_anisotropy = 0.0f;
        glossyPlastic.m_indexOfRefraction = 1.46f;
        glossyPlastic.m_transparency = 0.0f;
        registerMaterial(glossyPlastic);

        // Material 5: Polished Metal (Chrome)
        Material chrome = createDefaultMaterial();
        chrome.m_baseColor = float3(0.95f, 0.93f, 0.88f);
        chrome.m_metallic = 1.0f;
        chrome.m_roughness = 0.05f;
        chrome.m_anisotropy = 0.0f;
        chrome.m_indexOfRefraction = 2.5f;
        chrome.m_transparency = 0.0f;
        registerMaterial(chrome);

        // Material 6: Brushed Metal (Aluminum)
        Material aluminum = createDefaultMaterial();
        aluminum.m_baseColor = float3(0.91f, 0.92f, 0.92f);
        aluminum.m_metallic = 1.0f;
        aluminum.m_roughness = 0.35f;
        aluminum.m_anisotropy = 0.7f;
        aluminum.m_indexOfRefraction = 2.5f;
        aluminum.m_transparency = 0.0f;
        registerMaterial(aluminum);

        // Material 7: Rusted Metal
        Material rusted = createDefaultMaterial();
        rusted.m_baseColor = float3(0.6f, 0.3f, 0.1f);
        rusted.m_metallic = 0.3f;
        rusted.m_roughness = 0.9f;
        rusted.m_anisotropy = 0.0f;
        rusted.m_indexOfRefraction = 1.5f;
        rusted.m_transparency = 0.0f;
        registerMaterial(rusted);

        // Material 8: Ceramic
        Material ceramic = createDefaultMaterial();
        ceramic.m_baseColor = float3(0.95f, 0.95f, 0.9f);
        ceramic.m_metallic = 0.0f;
        ceramic.m_roughness = 0.1f;
        ceramic.m_anisotropy = 0.0f;
        ceramic.m_indexOfRefraction = 1.5f;
        ceramic.m_transparency = 0.0f;
        registerMaterial(ceramic);

        // Material 9: Rubber
        Material rubber = createDefaultMaterial();
        rubber.m_baseColor = float3(0.1f, 0.1f, 0.1f);
        rubber.m_metallic = 0.0f;
        rubber.m_roughness = 1.0f;
        rubber.m_anisotropy = 0.0f;
        rubber.m_indexOfRefraction = 1.5f;
        rubber.m_transparency = 0.0f;
        registerMaterial(rubber);

        // Material 10: Water
        Material water = createDefaultMaterial();
        water.m_baseColor = float3(0.8f, 0.9f, 1.0f);
        water.m_metallic = 0.0f;
        water.m_roughness = 0.0f;
        water.m_anisotropy = 0.0f;
        water.m_indexOfRefraction = 1.33f;
        water.m_transparency = 1.0f;
        registerMaterial(water);

        // Material 11: Diamond
        Material diamond = createDefaultMaterial();
        diamond.m_baseColor = float3(1.0f, 1.0f, 1.0f);
        diamond.m_metallic = 0.0f;
        diamond.m_roughness = 0.0f;
        diamond.m_anisotropy = 0.0f;
        diamond.m_indexOfRefraction = 2.42f;
        diamond.m_transparency = 1.0f;
        registerMaterial(diamond);

        // Material 12: Gold
        Material gold = createDefaultMaterial();
        gold.m_baseColor = float3(1.0f, 0.76f, 0.33f);
        gold.m_metallic = 1.0f;
        gold.m_roughness = 0.1f;
        gold.m_anisotropy = 0.0f;
        gold.m_indexOfRefraction = 1.5f;
        gold.m_transparency = 0.0f;
        registerMaterial(gold);

        // Material 13: Copper
        Material copper = createDefaultMaterial();
        copper.m_baseColor = float3(0.95f, 0.64f, 0.54f);
        copper.m_metallic = 1.0f;
        copper.m_roughness = 0.15f;
        copper.m_anisotropy = 0.0f;
        copper.m_indexOfRefraction = 1.5f;
        copper.m_transparency = 0.0f;
        registerMaterial(copper);
    }

    Material MaterialManager::createDefaultMaterial()
    {
        Material mat{};
        mat.m_baseColor = float3(0.5f, 0.5f, 0.5f);
        mat.m_metallic = 0.0f;
        mat.m_roughness = 0.5f;
        mat.m_indexOfRefraction = 1.5f;
        mat.m_transparency = 0.0f;
        mat.m_absorptionDensity = 1.0f;
        mat.m_id = 0;
        return mat;
    }

    uint MaterialManager::registerMaterial(Material material)
    {
        const uint slot = findNextAvailableSlot();
        if (slot >= kMaxMaterials) {
            printf("MaterialManager: No available slots!\n");
            return 0;
        }

        material.m_id = slot;
        m_aMaterials[slot] = material;
        m_aMaterialsInUse[slot] = true;
        m_materialCount++;

        return slot;
    }

    const Material& MaterialManager::getMaterial(const uint id) const
    {
        if (id >= kMaxMaterials) {
            return m_aMaterials[0];
        }
        return m_aMaterials[id];
    }

    void MaterialManager::updateMaterial(const uint id, const Material& material)
    {
        if (id < kMaxMaterials && m_aMaterialsInUse[id]) {
            m_aMaterials[id] = material;
            m_aMaterials[id].m_id = id;
        }
    }

    bool MaterialManager::isMaterialActive(const uint id) const
    {
        return id < kMaxMaterials && m_aMaterialsInUse[id];
    }

    std::vector<Material> MaterialManager::getAllActiveMaterials() const
    {
        std::vector<Material> activeMaterials;
        for (uint i = 0; i < kMaxMaterials; i++) {
            if (m_aMaterialsInUse[i]) {
                activeMaterials.push_back(m_aMaterials[i]);
            }
        }
        return activeMaterials;
    }

    uint MaterialManager::findNextAvailableSlot() const {
        // Find the first unused slot
        for (uint i = 0; i < kMaxMaterials; i++) {
            if (!m_aMaterialsInUse[i]) {
                return i;
            }
        }
        return kMaxMaterials;
    }

    const char* MaterialManager::getMaterialTypeName(const uint id)
    {
        switch (id) {
            case 0: return "Default";
            case 1: return "Mirror";
            case 2: return "Glass";
            case 3: return "Rough Plastic";
            case 4: return "Glossy Plastic";
            case 5: return "Chrome";
            case 6: return "Aluminum";
            case 7: return "Rusted Metal";
            case 8: return "Ceramic";
            case 9: return "Rubber";
            case 10: return "Water";
            case 11: return "Diamond";
            case 12: return "Gold";
            case 13: return "Copper";
            default: return "Custom";
        }
    }

}  // Tmpl8