#include "lighting.h"

#include "rt/scene/scene_loader.h"
#include "rt/lights/directional_light.h"
#include "rt/lights/point_light.h"
#include "rt/lights/spotlight.h"

namespace Tmpl8 {

    PointLight pointLight(float3(1), float3(1));
    DirectionalLight directionalLight(float3(-1, 1, 0));

    SpotLight spotLight(
        float3(0.5, 0.8f, 0.5),    // position
        float3(0, -1, 0),          // direction
        float3(1, 1, 1),           // color
        45.0f,                           // angle
        0.2f                             // soft band ratio
    );

    void LightRenderer::Init()
    {
        ProceduralSceneLoader loader;
        loader.Load(scene);

        pointLight.SetIntensity(0.5f);
        directionalLight.SetIntensity(1.0f);
    }

    float3 LightRenderer::Trace(Ray &ray, const int , const int , const int )
    {
        scene.FindNearest(ray);
        if (ray.voxel == 0) return {0.7f, 0.8f, 1};

        float3 totalLight = 0;

        const float3 directionalContribution = directionalLight.CalculateLightContribution(ray, scene);
        const float3 spotLightContribution = spotLight.CalculateLightContribution(ray, scene);
        const float3 pointLightContribution = pointLight.CalculateLightContribution(ray, scene);

        totalLight += directionalContribution + spotLightContribution + pointLightContribution;

        return totalLight;
    }

    void LightRenderer::Tick(const float deltaTime)
    {
        // pixel loop: lines are executed as OpenMP parallel tasks (disabled in DEBUG)
#pragma omp parallel for schedule(dynamic)
        for (int y = 0; y < SCRHEIGHT; y++)
        {
            // trace a primary ray for each pixel on the line
            for (int x = 0; x < SCRWIDTH; x++)
            {
                Ray r = camera.GetPrimaryRay( (float)x, (float)y );
                float3 pixel = Trace( r, 0, 0, 0);
                screen->pixels[x + y * SCRWIDTH] = RGBF32_to_RGB8( pixel );
            }
        }
        camera.HandleInput( deltaTime );
    }

    void LightRenderer::UI()
    {
        // Spotlight
        if (ImGui::CollapsingHeader("Spot Light Attributes")) {
            float intensity = spotLight.GetIntensity();
            if (ImGui::SliderFloat("Intensity##spotlight", &intensity, 0, 10)) {
                spotLight.SetIntensity(intensity);
            }

            float3 color = spotLight.GetColor();
            if (ImGui::ColorEdit3("Color##spotlight", reinterpret_cast<float*>(&color))) {
                spotLight.SetColor(color);
            }

            ImGui::Separator();

            float3 position = spotLight.GetPosition();
            if (ImGui::DragFloat3("Position##spotlight", reinterpret_cast<float*>(&position), 0.05f)) {
                spotLight.SetPosition(position);
            }

            float3 direction = spotLight.GetDirection();
            if (ImGui::DragFloat3("Direction##spotlight", reinterpret_cast<float*>(&direction), 0.05f, -1.0f, 1.0f)) {
                spotLight.SetDirection(direction);
            }

            ImGui::Separator();

            float pitch = spotLight.GetPitch();
            if (ImGui::DragFloat("Pitch##spotlight", &pitch, 0.1f, -90.0f, 90.0f)) {
                spotLight.SetPitch(pitch);
            }

            float yaw = spotLight.GetYaw();
            if (ImGui::DragFloat("Yaw##spotlight", &yaw, 0.1f)) {
                spotLight.SetYaw(yaw);
            }

            ImGui::Separator();

            float angle = spotLight.GetSpotAngle();
            if (ImGui::DragFloat("Angle##spotlight", &angle, 0.1f, 0, 180)) {
                spotLight.SetSpotAngle(angle);
            }

            float softBand = spotLight.GetSoftBandRatio();
            if (ImGui::DragFloat("Soft Band Ratio##spotlight", &softBand, 0.01f, 0, 1)) {
                spotLight.SetSoftBandRatio(softBand);
            }
        }

        if (ImGui::CollapsingHeader("Directional Light Attributes")) {
            float intensity = directionalLight.GetIntensity();
            if (ImGui::SliderFloat("Intensity##directional_light", &intensity, 0, 10)) {
                directionalLight.SetIntensity(intensity);
            }

            float3 color = directionalLight.GetColor();
            if (ImGui::ColorEdit3("Color##directional_light", reinterpret_cast<float*>(&color))) {
                directionalLight.setColor(color);
            }

            float3 direction = directionalLight.GetDirection();
            if (ImGui::DragFloat3("Direction##directional_light", reinterpret_cast<float*>(&direction), 0.05f, -1.0f, 1.0f)) {
                directionalLight.SetDirection(direction);
            }
        }

        // Point light
        if (ImGui::CollapsingHeader("Point Light Attributes")) {
            float intensity = pointLight.GetIntensity();
            if (ImGui::SliderFloat("Intensity##point_light", &intensity, 0, 5)) {
                pointLight.SetIntensity(intensity);
            }

            float3 color = pointLight.GetColor();
            if (ImGui::ColorEdit3("Color##point_light", reinterpret_cast<float*>(&color))) {
                pointLight.SetColor(color);
            }

            float3 position = pointLight.GetPosition();
            if (ImGui::DragFloat3("Position##point_light", reinterpret_cast<float*>(&position), 0.05f)) {
                pointLight.SetPosition(position);
            }

            ImGui::Separator();

            if (ImGui::Button("Reset point light")) {
                pointLight = PointLight(float3(1), float3(1));
                pointLight.SetIntensity(0.5f);
            }
        }
    }

    void LightRenderer::Shutdown()
    {
        Renderer::Shutdown();
    }

    void LightRenderer::MouseUp(const int button)
    {
        Renderer::MouseUp(button);
    }

    void LightRenderer::MouseDown(const int button)
    {
        Renderer::MouseDown(button);
    }

    void LightRenderer::MouseMove(const int x, const int y)
    {
        Renderer::MouseMove(x, y);
    }

    void LightRenderer::MouseWheel(const float y)
    {
        Renderer::MouseWheel(y);
    }

    void LightRenderer::KeyUp(const int key)
    {
        Renderer::KeyUp(key);
    }

    void LightRenderer::KeyDown(const int key)
    {
        Renderer::KeyDown(key);
    }

}  // Tmpl8