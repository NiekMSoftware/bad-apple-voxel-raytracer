#pragma once

#include "rt/rendering/material_editor.h"
#include "rt/rendering/material_manager.h"
#include "rt/core/renderer.h"
#include "rt/management/ui_manager.h"

namespace Tmpl8 {

    enum MaterialType { DIFFUSE, MIRROR, GLASS };

    class ReflectionsRenderer : public Renderer
    {
    public:
        ReflectionsRenderer();

        void Init() override;
        float3 Trace( Ray& ray, int, int, int ) override;

        float3 TracePBR(Ray& ray, int depth, float3 throughput = float3(1.0f));
        void Tick( float deltaTime ) override;
        void UI() override;
        void Shutdown() override;

    private:
        int spp = 0;
        const int MAX_SPP = 512;
        float ambientFactor = 0.03f;

        Management::LightManager lightManager;
        MaterialManager materialManager;
        Management::UIManager uiManager;

        Management::DiagnosticsData lastDiag;

        static MaterialType GetMaterialType(uint voxel);

        float3 TraceIndirectSpecular(const Ray & ray, const Material & material,
                    float3 N, float3 V, float3 albedo,
                    float NdotV, int depth, float3 throughput);
        float3 TraceTransparent(Ray & ray, Material & material, float3 N, float3 V, float3 albedo, int depth);

        void ResetAccumulator();
    };

} // Tmpl8