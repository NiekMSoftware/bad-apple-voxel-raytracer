#pragma once
#include "material.h"

namespace rt::rendering {

    class MaterialManager 
    {
    public:
        static constexpr uint kMaxMaterials = 256;
        uint m_selectedMaterialIndex = 0;

        MaterialManager();
        ~MaterialManager() = default;

        void initialize();

        uint registerMaterial(Material material);
        static Material createDefaultMaterial();
        [[nodiscard]] const Material& getMaterial(uint id) const;

        void updateMaterial(uint id, const Material& material);
        [[nodiscard]] uint getMaterialCount() const { return m_materialCount; }
        [[nodiscard]] bool isMaterialActive(uint id) const;

        [[nodiscard]] std::vector<Material> getAllActiveMaterials() const;
        [[nodiscard]] static const char* getMaterialTypeName(uint id);
    private:
        Material m_aMaterials[kMaxMaterials];
        bool m_aMaterialsInUse[kMaxMaterials] = { false };
        uint m_materialCount = 0;

        [[nodiscard]] uint findNextAvailableSlot() const;
    };

}  // rt::rendering