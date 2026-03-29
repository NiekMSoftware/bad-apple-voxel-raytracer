#include "projection_renderer.h"

#include "rt/scene/scene_loader.h"
#include "tmpl8/tmpl8math.h"

namespace Tmpl8::Projection {

    ProjectionRenderer::ProjectionRenderer()
        : uiManager(lightManager, materialManager)
    { }

    void ProjectionRenderer::Init()
    {
        FileSceneLoader loader( "assets/viking.bin" );
        loader.Load( scene );

        accumulator = new float3[SCRWIDTH * SCRHEIGHT];
        memset( accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof( float3 ) );
    }

    float3 ProjectionRenderer::Trace(Ray &ray) const {
        scene.FindNearest( ray );
        if (ray.voxel == 0) return { 0.4f, 0.5f, 1.0f };

        return lightManager.CalculateLighting( ray, scene );
    }

    void ProjectionRenderer::ResetAccumulator()
    {
        memset(accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
        spp = 0;
    }

    void ProjectionRenderer::Tick( const float deltaTime )
    {
        // frame diagnostic timer
        Timer t;

        // optimisation by Lynn van Birgelen
        // https://jacco.ompf2.com/2024/05/08/ray-tracing-with-voxels-in-c-series-part-3/
        const float accumulatorWeight = static_cast<float>(spp) / static_cast<float>(spp + 1);
        const float newSampleWeight = 1.0f - accumulatorWeight;
        spp++;

#pragma omp parallel for schedule(dynamic)
        for (int y = 0; y < SCRHEIGHT; y++)
        {
            for (int x = 0; x < SCRWIDTH; x++)
            {
                Ray r = camera.GetPrimaryRay( static_cast<float>(x) + RandomFloat(), static_cast<float>(y) + RandomFloat() );
                float3 newSample = Trace(r);
                float3& accumulatedPixel = accumulator[x + y * SCRWIDTH];

                // find the average result
                float3 finalPixel = newSample * newSampleWeight + accumulatedPixel * accumulatorWeight;

                // store in the accumulator buffer
                accumulatedPixel = finalPixel;

                screen->pixels[x + y * SCRWIDTH] = RGBF32_to_RGB8( finalPixel );
            }
        }

        spp = min(spp + 1, 8196);

        // Running-average frame time (same logic as original template)
        static float avg = 10, alpha = 1;
        avg = (1 - alpha) * avg + alpha * t.elapsed() * 1000.0f;
        if (alpha > 0.05f) alpha *= 0.5f;

        // Fill diagnostics for the UI
        lastDiag.frameTimeMs  = avg;
        lastDiag.fps          = avg > 0.0f ? 1000.0f / avg : 0.0f;
        lastDiag.mraysPerSec  = static_cast<float>((SCRWIDTH * SCRHEIGHT)) / avg / 1000.0f;
        lastDiag.cameraPos    = camera.camPos;
        lastDiag.cameraDir    = normalize(camera.camTarget - camera.camPos);

        if (camera.HandleInput( deltaTime )) ResetAccumulator();
    }

    void ProjectionRenderer::UI()
    {
        if (uiManager.Render(lastDiag))
            ResetAccumulator();

        ImGui::Separator();
        ImGui::Text( "Samples: %d", spp - 1 );
    }

    void ProjectionRenderer::Shutdown()
    {
        if (accumulator)
        {
            delete[] accumulator;
            accumulator = nullptr;
        }
    }

}  // Tmpl8