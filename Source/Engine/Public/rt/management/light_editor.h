#pragma once
#include "light_manager.h"

namespace rt::management {

    class LightEditor
    {
    public:
        LightEditor() = default;
        ~LightEditor() = default;

        bool render(LightManager& lm);

        void show() { m_bVisible = true;}
        void hide() { m_bVisible = false; }
        void toggle() { m_bVisible = !m_bVisible; }
        [[nodiscard]] bool isVisible() const { return m_bVisible; }

    private:
        bool m_bVisible = true;

        int m_selDir = -1;
        int m_selPt = -1;
        int m_selSpot = -1;

        bool renderDirectionalSection(LightManager& lm);
        bool renderPointSection(LightManager& lm);
        bool renderSpotSection(LightManager& lm);
    };

}  // namespace Tmpl8::Management