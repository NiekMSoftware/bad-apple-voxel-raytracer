#pragma once

#include "tmpl8/app.h"
#include "rt/core/camera.h"
#include "rt/core/ray.h"
#include "rt/scene/scene.h"

#include "rt/management/light_manager.h"
#include "rt/management/ui_manager.h"

namespace Tmpl8::Projection {

    class ProjectionRenderer : public Application
    {
    public:
        ProjectionRenderer();

        void Init() override;
        float3 Trace( Ray& ray) const;
        void Tick( float deltaTime ) override;
        void UI() override;
        void Shutdown() override;

    private:
        void ResetAccumulator();
        int spp = 0;

        float3* accumulator = nullptr;
        Scene scene;
        Camera camera;

        Management::LightManager lightManager;
        MaterialManager materialManager;
        Management::UIManager uiManager;

        Management::DiagnosticsData lastDiag;
    };

}  // namespace Tmpl8::Projection