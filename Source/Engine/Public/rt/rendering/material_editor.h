#pragma once

#include "material_manager.h"

namespace rt::rendering {

    class MaterialEditor 
    {
    public:
        MaterialEditor() = default;
        ~MaterialEditor() = default;

        // Main render function - call this from your UI() method
        bool render(MaterialManager& materialManager);

        // Toggle visibility
        void show() { m_bVisible = true; }
        void hide() { m_bVisible = false; }
        void toggle() { m_bVisible = !m_bVisible; }
        [[nodiscard]] bool isVisible() const { return m_bVisible; }

    private:
        bool m_bVisible = false;

        // Internal UI sections
        static void renderMaterialList(MaterialManager& materialManager);
        static bool renderPropertyEditor(MaterialManager& materialManager);
        static void renderPresetButtons(Material& material, bool& hasChanged);
        static void renderMaterialPreview(const Material& material);
    };

}  // rt::rendering