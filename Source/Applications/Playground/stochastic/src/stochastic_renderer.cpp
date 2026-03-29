#include "stochastic_renderer.h"

#include "tmpl8/tmpl8math.h"
#include "rt/scene/scene_loader.h"
#include "rt/lights/directional_light.h"
#include "rt/lights/point_light.h"
#include "rt/lights/spotlight.h"

namespace Tmpl8 {

    DirectionalLight dirLight( float3( -1, 1, 0 ) );
    PointLight pointLight( float3( 0.7f, 0.7f, 0.35f ), float3( 1 ) );
    SpotLight spotLight(
        float3( 0.5f, 0.8f, 0.5f ),
        float3( 0, -1, 0 ),
        float3( 1, 1, 1 ),
        45.0f,
        0.2f
    );

    void StochasticRenderer::Init()
    {
        FileSceneLoader loader( "assets/viking.bin" );
        loader.Load( scene );

        accumulator = new float3[SCRWIDTH * SCRHEIGHT];
        memset( accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof( float3 ) );

        dirLight.SetIntensity( 1.0f );
        pointLight.SetIntensity( 0.5f );
        spotLight.SetIntensity( 1.0f );
    }

    float3 StochasticRenderer::Trace( Ray& ray, const int, const int, const int )
    {
        scene.FindNearest( ray );
        if (ray.voxel == 0) return { 0.4f, 0.5f, 1.0f };

        float3 totalLight( 0 );
        totalLight += dirLight.CalculateLightContribution( ray, scene );
        totalLight += pointLight.CalculateLightContribution( ray, scene );
        totalLight += spotLight.CalculateLightContribution( ray, scene );

        return totalLight;
    }

    void StochasticRenderer::ResetAccumulator()
    {
        spp = 0;
    }

    void StochasticRenderer::Tick( const float deltaTime )
    {
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
                float3 newSample = Trace(r, 0, 0, 0);
                float3& accumulatedPixel = accumulator[x + y * SCRWIDTH];

                // find the average result
                float3 finalPixel = newSample * newSampleWeight + accumulatedPixel * accumulatorWeight;

                // store in the accumulator buffer
                accumulatedPixel = finalPixel;

                screen->pixels[x + y * SCRWIDTH] = RGBF32_to_RGB8( finalPixel );
            }
        }

        if (camera.HandleInput( deltaTime )) ResetAccumulator();
    }

    void StochasticRenderer::UI()
    {
        // Directional light
        if (ImGui::CollapsingHeader( "Directional Light" ))
        {
            float intensity = dirLight.GetIntensity();
            if (ImGui::SliderFloat( "Intensity##dir", &intensity, 0, 10 ))
                dirLight.SetIntensity( intensity ), ResetAccumulator();

            float3 color = dirLight.GetColor();
            if (ImGui::ColorEdit3( "Color##dir", reinterpret_cast<float*>( &color ) ))
                dirLight.setColor( color ), ResetAccumulator();

            float3 direction = dirLight.GetDirection();
            if (ImGui::DragFloat3( "Direction##dir", reinterpret_cast<float*>( &direction ), 0.05f, -1.0f, 1.0f ))
                dirLight.SetDirection( direction ), ResetAccumulator();

            float dirSoftness = dirLight.GetAngularRadius();
            if (ImGui::DragFloat( "Softness##dir", &dirSoftness, 0.01f, 0.0f, 1.0f ))
                dirLight.SetAngularRadius( dirSoftness ), ResetAccumulator();
        }

        // Point light
        if (ImGui::CollapsingHeader( "Point Light" ))
        {
            float intensity = pointLight.GetIntensity();
            if (ImGui::SliderFloat( "Intensity##point", &intensity, 0, 5 ))
                pointLight.SetIntensity( intensity ), ResetAccumulator();

            float3 color = pointLight.GetColor();
            if (ImGui::ColorEdit3( "Color##point", reinterpret_cast<float*>( &color ) ))
                pointLight.SetColor( color ), ResetAccumulator();

            float3 position = pointLight.GetPosition();
            if (ImGui::DragFloat3( "Position##point", reinterpret_cast<float*>( &position ), 0.05f ))
                pointLight.SetPosition( position ), ResetAccumulator();

            float pointSoftness = pointLight.GetRadius();
            if (ImGui::DragFloat( "Softness##point", &pointSoftness, 0.01f, 0.0f, 0.5f ))
                pointLight.SetRadius( pointSoftness ), ResetAccumulator();
        }

        // Spotlight
        if (ImGui::CollapsingHeader( "Spot Light" ))
        {
            float intensity = spotLight.GetIntensity();
            if (ImGui::SliderFloat( "Intensity##spot", &intensity, 0, 10 ))
                spotLight.SetIntensity( intensity ), ResetAccumulator();

            float3 color = spotLight.GetColor();
            if (ImGui::ColorEdit3( "Color##spot", reinterpret_cast<float*>( &color ) ))
                spotLight.SetColor( color ), ResetAccumulator();

            float3 position = spotLight.GetPosition();
            if (ImGui::DragFloat3( "Position##spot", reinterpret_cast<float*>( &position ), 0.05f ))
                spotLight.SetPosition( position ), ResetAccumulator();

            float spotSoftness = spotLight.GetRadius();
            if (ImGui::DragFloat( "Softness##spot", &spotSoftness, 0.01f, 0.0f, 0.5f ))
                spotLight.SetRadius( spotSoftness ), ResetAccumulator();

            float pitch = spotLight.GetPitch();
            if (ImGui::DragFloat( "Pitch##spot", &pitch, 0.1f, -90.0f, 90.0f ))
                spotLight.SetPitch( pitch ), ResetAccumulator();

            float yaw = spotLight.GetYaw();
            if (ImGui::DragFloat( "Yaw##spot", &yaw, 0.1f ))
                spotLight.SetYaw( yaw ), ResetAccumulator();

            float angle = spotLight.GetSpotAngle();
            if (ImGui::DragFloat( "Angle##spot", &angle, 0.1f, 0, 180 ))
                spotLight.SetSpotAngle( angle ), ResetAccumulator();

            float softBand = spotLight.GetSoftBandRatio();
            if (ImGui::DragFloat( "Soft Band##spot", &softBand, 0.01f, 0, 1 ))
                spotLight.SetSoftBandRatio( softBand ), ResetAccumulator();
        }

        ImGui::Separator();
        ImGui::Text( "Samples: %d", spp - 1 );
    }

    void StochasticRenderer::Shutdown()
    {
        if (accumulator)
        {
            delete[] accumulator;
            accumulator = nullptr;
        }
    }

    void StochasticRenderer::MouseUp( const int button ) { Renderer::MouseUp( button ); }
    void StochasticRenderer::MouseDown( const int button ) { Renderer::MouseDown( button ); }
    void StochasticRenderer::MouseMove( const int x, const int y ) { Renderer::MouseMove( x, y ); }
    void StochasticRenderer::MouseWheel( const float y ) { Renderer::MouseWheel( y ); }
    void StochasticRenderer::KeyUp( const int key ) { Renderer::KeyUp( key ); }
    void StochasticRenderer::KeyDown( const int key ) { Renderer::KeyDown( key ); }

}  // Tmpl8