#pragma once

// ============================================================================
// scene_builder.h
//
// Concept: a dark voxel theatre.
//
//   The Bad Apple wall (Z=256, X[0..511], Y[0..511]) acts as the screen.
//   Analytic spheres sit on the floor in front of it like an audience.
//   Three spotlights hang above the stage and light the floor and spheres.
//   The checkerboard floor uses mirror dark tiles so the screen reflects
//   on the ground between the seats.
//
//   One large ceramic sphere (DoF subject) sits close to the camera at
//   Z=80, centred at X=256.  The camera pans to it for the DoF segment.
//   Ceramic (mat 8) is opaque and non-metallic so it is cheap to shade.
//
// All world positions are in normalised [0,1] space (divide voxels by 512).
// Wall is at Z = 256/512 = 0.500.  Camera lives at Z < 0.500.
//
// Material IDs (MaterialManager::initialize(), material_manager.cpp):
//   0  Default diffuse   grey       roughness 0.50
//   1  Mirror            silver     roughness 0.00  metallic
//   2  Glass             white      transparent     IOR 1.50
//   3  Rough plastic     red        roughness 0.80
//   4  Glossy plastic    blue       roughness 0.20
//   5  Chrome            silver     roughness 0.05  metallic
//   6  Brushed aluminium grey       roughness 0.35  metallic  aniso 0.70
//   7  Rusted metal      brown      roughness 0.90  metallic 0.3
//   8  Ceramic           cream      roughness 0.10
//   9  Rubber            black      roughness 1.00
//  10  Water             blue-white transparent     IOR 1.33
//  11  Diamond           white      transparent     IOR 2.42
//  12  Gold              yellow     roughness 0.10  metallic
//  13  Copper            orange     roughness 0.15  metallic
// ============================================================================

#include "rt/lights/point_light.h"
#include "rt/lights/spotlight.h"
#include "rt/management/light_manager.h"
#include "rt/rendering/bit_packer.h"
#include "rt/rendering/primitives/analytic_scene.h"
#include "rt/rendering/primitives/sphere.h"
#include "rt/scene/scene.h"
#include "tmpl8/template.h"

namespace demo {

    // -------------------------------------------------------------------------
    // Geometry constants (voxel-integer space, WORLDSIZE=512)
    // -------------------------------------------------------------------------

    constexpr uint k_wallZ = 256;

    // Audience rows
    // Row 0  Z=130  front  -- opaque non-metallic: ceramic, rough plastic, glossy plastic, diffuse, rubber
    // Row 1  Z=175  middle -- mixed opaque: diffuse, glossy plastic, rubber, rough plastic
    // Row 2  Z=220  back   -- one glass each end, opaque centre, to reduce transparent sphere count
    constexpr uint  k_rowCount      = 3;
    constexpr uint  k_rowZ[3]       = { 130, 175, 220 };
    constexpr uint  k_spheresPerRow = 7;
    constexpr uint  k_rowXStart     = 110;
    constexpr uint  k_rowXStep      = 48;
    constexpr float k_sphereRadius  = 10.0f / 512.0f;

    // Row 0: all opaque non-metallic -- cheap to shade, good for DoF background
    //   8(ceramic), 3(rough plastic), 4(glossy plastic), 0(diffuse), 9(rubber), 4(glossy), 3(rough)
    // Row 1: mix of opaque non-metallic and one mirror for floor reflection interest
    //   0(diffuse), 4(glossy), 9(rubber), 1(mirror), 9(rubber), 4(glossy), 0(diffuse)
    // Row 2: reduced transparent -- only col 0 and col 6 are glass, rest opaque
    //   2(glass), 8(ceramic), 0(diffuse), 9(rubber), 0(diffuse), 8(ceramic), 2(glass)
    constexpr uint k_rowMat[3][7] = {
        { 8,  3,  4,  0,  9,  4,  3 },
        { 0,  4,  9,  1,  9,  4,  0 },
        { 2,  8,  0,  9,  0,  8,  2 }
    };

    // -------------------------------------------------------------------------
    // DoF subject sphere
    //
    // One large ceramic sphere placed close to the camera at Z=80.
    // It sits at X=256 (centre) so the camera only needs a gentle pan to
    // bring it into frame.  Radius=20 voxels makes it prominent in frame
    // without requiring the camera to move close to it.
    //
    // Ceramic (mat 8): opaque, non-metallic, roughness 0.10.
    // shadeOpaqueSphere / shadeVoxel path -- no refraction rays fired.
    // -------------------------------------------------------------------------
    constexpr uint  k_dofSphereX_vox = 256;
    constexpr uint  k_dofSphereZ_vox = 80;
    constexpr float k_dofSphereRadius = 20.0f / 512.0f;
    constexpr uint  k_dofSphereMat    = 8;  // ceramic

    // World-space centre of the DoF sphere (for focal distance calculation)
    constexpr float k_dofSphereX = static_cast<float>(k_dofSphereX_vox) / 512.0f;
    constexpr float k_dofSphereY = k_dofSphereRadius;  // rests on floor
    constexpr float k_dofSphereZ = static_cast<float>(k_dofSphereZ_vox) / 512.0f;

    // =========================================================================
    // buildTheatreFloor
    // =========================================================================
    inline void buildTheatreFloor(rt::scene::Scene& scene)
    {
        constexpr uint xMin = 50;
        constexpr uint xMax = 462;
        constexpr uint zMin = 50;
        constexpr uint zMax = 462;
        constexpr uint tile = 16;

        for (uint x = xMin; x < xMax; x++)
        for (uint z = zMin; z < zMax; z++)
        {
            const bool light = ((x / tile) + (z / tile)) % 2 == 0;
            const uint v = light
                ? rt::rendering::packVoxel(0, 210, 210, 210)
                : rt::rendering::packVoxel(1,  30,  30,  30);
            scene.set(x, 0, z, v);
        }
    }

    // =========================================================================
    // buildAudienceSpheres
    //
    // 21 audience spheres in 3 rows plus 1 large DoF subject sphere.
    // =========================================================================
    inline void buildAudienceSpheres(rt::primitives::AnalyticScene& analyticScene)
    {
        constexpr float inv = 1.0f / 512.0f;

        // Audience rows
        for (uint row = 0; row < k_rowCount; row++)
        {
            for (uint col = 0; col < k_spheresPerRow; col++)
            {
                rt::primitives::Sphere s;
                s.m_radius   = k_sphereRadius;
                s.m_center.x = static_cast<float>(k_rowXStart + col * k_rowXStep) * inv;
                s.m_center.y = k_sphereRadius;
                s.m_center.z = static_cast<float>(k_rowZ[row]) * inv;
                s.m_matIndex = k_rowMat[row][col];
                analyticScene.addSphere(s);
            }
        }
    }

    // =========================================================================
    // buildTheatreLights
    // =========================================================================
    inline void buildTheatreLights(rt::management::LightManager& lights)
    {
        // Left spotlight
        {
            rt::lights::SpotLight s(
                float3(0.22f, 1.0f, 0.38f),
                float3(0.0f, -1.0f, 0.15f),
                float3(0.0f, 0.0f, 1.0f),
                30.0f,
                0.25f
            );
            s.setIntensity(2.5f);
            s.setRadius(0.008f);
            s.setSpotAngle(45.0f);
            lights.addSpotLight(s);
        }

        // Centre spotlight
        {
            rt::lights::SpotLight s(
                float3(0.50f, 1.5f, 0.38f),
                float3(0.0f, -1.0f, 0.10f),
                float3(0.0f, 1.0f, 0.0f),
                28.0f,
                0.20f
            );
            s.setIntensity(2.8f);
            s.setRadius(0.008f);
            s.setSpotAngle(45.0f);
            lights.addSpotLight(s);
        }

        // Right spotlight
        {
            rt::lights::SpotLight s(
                float3(0.78f, 1.0f, 0.38f),
                float3(0.0f, -1.0f, 0.15f),
                float3(1.0f, 0.0f, 0.0f),
                30.0f,
                0.25f
            );
            s.setIntensity(2.5f);
            s.setRadius(0.008f);
            s.setSpotAngle(45.0f);
            lights.addSpotLight(s);
        }

        // Screen fill -- dim point light just in front of the wall
        {
            rt::lights::PointLight p(
                float3(0.50f, 0.50f, 0.47f),
                float3(1.00f, 1.00f, 1.00f)
            );
            p.setIntensity(0.0f);
            p.setRadius(0.0f);
            lights.addPointLight(p);
        }
    }

    // =========================================================================
    // buildStaticCubeInto
    // (preserved so existing call sites compile)
    // =========================================================================
    constexpr int k_cubeCX   = 256;
    constexpr int k_cubeCY   = 55;
    constexpr int k_cubeCZ   = 256;
    constexpr int k_cubeHalf = 12;

    constexpr uint k_matBrushed = 6;
    constexpr uint k_voxBrushed = (k_matBrushed << 24) | (232u << 16) | (235u << 8) | 235u;

    inline void buildStaticCubeInto(const rt::scene::BrickPool& blas)
    {
        for (int dj = -k_cubeHalf; dj < k_cubeHalf; dj++)
        for (int di = -k_cubeHalf; di < k_cubeHalf; di++)
        for (int dk = -k_cubeHalf; dk < k_cubeHalf; dk++)
        {
            const int wx = k_cubeCX + di;
            const int wy = k_cubeCY + dj;
            const int wz = k_cubeCZ + dk;
            blas.set(wx, wy, wz, k_voxBrushed);
        }
    }

}  // namespace demo