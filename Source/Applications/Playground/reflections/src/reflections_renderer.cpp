#include "reflections_renderer.h"

#include "rt/brdf/brdf.h"
#include "rt/core/optics.h"

#include "rt/rendering/bit_packer.h"
#include "rt/rendering/material.h"

#include "room_loader.h"
#include "rt/lights/point_light.h"

namespace Tmpl8 {

    ReflectionsRenderer::ReflectionsRenderer()
        : uiManager(lightManager, materialManager)
    { }

    void ReflectionsRenderer::Init() {
        RoomLoader loader;
        loader.Load(scene);

        materialManager.Initialize();

        accumulator = new float3[SCRWIDTH * SCRHEIGHT];
        memset(accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
    }

    float3 ReflectionsRenderer::Trace(Ray &ray, const int depth, int , int ) {
        return TracePBR(ray, depth);
    }

    float3 ReflectionsRenderer::TracePBR(Ray &ray, const int depth, float3 throughput) {
        if (depth == 4)  return {0};

        // russian roulette
        if (depth >= 2) {
            const float p = max(throughput.x, max(throughput.y, throughput.z));
            if (p < EPSILON) return {0};
            if (RandomFloat() > p) return {0};
            throughput /= p;
        }

        scene.FindNearest(ray);

        if (ray.voxel == 0) return {0.4f, 0.5f, 1.0f};

        // extract material data
        const uint materialID = GetMaterialID(ray.voxel);
        Material material = materialManager.GetMaterial(materialID);

        // Extract surface data
        const float3 N = ray.GetNormal();
        const float3 I = ray.IntersectionPoint();
        const float3 albedo = material.baseColor;
        const float3 V = normalize(-ray.D);
        const float NdotV = max(dot(N, V), 0.0f);

        // Initialize output radians
        float3 Lo{0};

        if (material.transparency > 0.0f) {
            Lo = TraceTransparent(ray, material, N, V, albedo, depth);
            return Lo;
        }

        Lo += lightManager.CalculateLighting(I, N, V, material, scene, materialManager);

        if (material.roughness < 0.8f || material.metallic > 0.5f) {
            Lo += TraceIndirectSpecular(ray, material, N, V, albedo, NdotV, depth, throughput);
        }

        const float3 ambient = albedo * ambientFactor * (1.0f - material.metallic);
        Lo += ambient;

        return Lo;
    }

    void ReflectionsRenderer::Tick(const float deltaTime) {
        Timer frameTimer;

        const float accumulatorWeight = static_cast<float>(spp) / static_cast<float>(spp + 1);
        const float newSampleWeight = 1.0f - accumulatorWeight;

#pragma omp parallel for schedule(dynamic)
        for (int y = 0; y < SCRHEIGHT; y++)
        {
            // trace a primary ray for each pixel on the line
            for (int x = 0; x < SCRWIDTH; x++)
            {
                Ray r = camera.GetPrimaryRay( static_cast<float>(x) + RandomFloat(), static_cast<float>(y) + RandomFloat() );
                float3 newSample = Trace( r, 0, 0, 0);
                float3& accumulatedPixel = accumulator[x + y * SCRWIDTH];

                // find the average result
                float3 finalPixel = newSample * newSampleWeight + accumulatedPixel * accumulatorWeight;
                accumulatedPixel = finalPixel;

                screen->pixels[x + y * SCRWIDTH] = RGBF32_to_RGB8( finalPixel );
            }
        }

        // Running-average frame time
        static float avg = 10, alpha = 1;
        avg = (1 - alpha) * avg + alpha * frameTimer.elapsed() * 1000.0f;
        if (alpha > 0.05f) alpha *= 0.5f;

        // Fill diagnostics for the UI
        lastDiag.frameTimeMs  = avg;
        lastDiag.fps          = avg > 0.0f ? 1000.0f / avg : 0.0f;
        lastDiag.mraysPerSec  = static_cast<float>((SCRWIDTH * SCRHEIGHT)) / avg / 1000.0f;
        lastDiag.cameraPos    = camera.camPos;
        lastDiag.cameraDir    = normalize(camera.camTarget - camera.camPos);

        spp = min(spp + 1, MAX_SPP);
        if (camera.HandleInput( deltaTime )) ResetAccumulator();
    }

    void ReflectionsRenderer::UI() {
        if (ImGui::SliderFloat("Ambient", &ambientFactor, 0.0f, 1.0f))
            ResetAccumulator();

        if (uiManager.Render(lastDiag))
            ResetAccumulator();
    }

    void ReflectionsRenderer::Shutdown() {
        delete[] accumulator;
    }

    MaterialType ReflectionsRenderer::GetMaterialType(const uint voxel) {
        // Extract the material identifier from the upper 8 bits
        uint materialId = voxel >> 24;
        return static_cast<MaterialType>(materialId);
    }

    float3 ReflectionsRenderer::TraceIndirectSpecular(const Ray &ray, const Material &material, const float3 N, const float3 V,
        const float3 albedo, const float NdotV, const int depth, float3 throughput)
    {
        const float3 F0 = brdf::CalculateF0(albedo, material.metallic, material.indexOfRefraction);
        const float3 F = brdf::FresnelSchlick(NdotV, F0);
        const float roughnessAttenuation = 1.0f - material.roughness;

        // early exit: if the total contribution would be negligible, skip the trace
        const float maxContrib = max(F.x, max(F.y, F.z)) * roughnessAttenuation;
        if (maxContrib < 0.01f) return {0};

        const float3 newThroughput = throughput * F * roughnessAttenuation;

        const float3 R = Reflect(-V, N);
        Ray reflectionRay(ray.IntersectionPoint(), R);
        const float3 reflectedColor = TracePBR(reflectionRay, depth + 1, newThroughput);

        const float3 weight = F * roughnessAttenuation;
        return reflectedColor * weight;
    }

    float3 ReflectionsRenderer::TraceTransparent(
        Ray &ray, Material &material,
        float3 N, float3 V,
        float3 albedo, int depth
    ) {
        float ior = material.indexOfRefraction;
        float cosI = dot(N, V);

        float eta{0};
        if (cosI < 0.0f) {
            N = -N;
            cosI = -cosI;
            eta = ior / 1.0f;
        } else {
            eta = 1.0f / ior;
        }

        float3 I = ray.IntersectionPoint();

        float sinTSq = eta * eta * (1.0f - cosI * cosI);
        if (sinTSq > 1.0f) {
            float3 R = Reflect(-V, N);
            Ray reflectionRay(I, R);
            return albedo * TracePBR(reflectionRay, depth + 1);
        }

        float cosT = sqrtf(1.0f - sinTSq);
        float3 T = eta * (-V) + (eta * cosI - cosT) * N;
        T = normalize(T);

        float3 F0 = pow2f((1.0f - ior) / (1.0f + ior));
        float3 F = F0 + (1.0f - F0) * pow5f(1.0f - cosI);

        // Trace both reflection and refraction
        float3 R = Reflect(-V, N);

        Ray reflectionRay(I, R);
        Ray refractionRay(I, T);

        float3 reflectedColor = TracePBR(reflectionRay, depth + 1);
        float3 refractedColor = TracePBR(refractionRay, depth + 1);

        float3 finalColor = F * reflectedColor + (1.0f - F) * refractedColor;
        finalColor *= albedo;

        return finalColor;
    }

    void ReflectionsRenderer::ResetAccumulator() {
        spp = 0;
        memset(accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
    }

}  // Tmpl8

// EoF