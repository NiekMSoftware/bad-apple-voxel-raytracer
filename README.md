# Voxpopuli

Voxpopuli is a voxel-based CPU path tracer written in C++17. It renders a sparse voxel world alongside analytic primitives (spheres) and triangle meshes, with physically based shading, multiple light types, HDR environment lighting, and temporal reprojection.

For the original version that Jacco Bikker made, check out: https://github.com/jbikker/voxpopuli

[![Voxpopuli Demo](https://img.youtube.com/vi/wcOL_cHXLmM/maxresdefault.jpg)](https://youtu.be/wcOL_cHXLmM)


## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Architecture](#architecture)
4. [Requirements](#requirements)
5. [Building](#building)
6. [Usage](#usage)
7. [Project Structure](#project-structure)
8. [Coding Conventions](#coding-conventions)
9. [References](#references)
10. [License](#license)


## Overview

The renderer traces primary rays through a two-level sparse voxel grid (a brick map), tests analytic spheres via a BVH built with tinyBVH, and evaluates triangle meshes through the same BVH library. Shading uses a Cook-Torrance microfacet BRDF with GGX distribution, Smith geometry, and Schlick Fresnel, supporting both isotropic and anisotropic materials. The accumulator blends samples over time with temporal reprojection and variance clipping when the camera moves.


## Features

### Rendering

- Iterative path tracing with configurable maximum bounce depth (default 4) and Russian roulette termination.
- Progressive sample accumulation up to 4096 samples per pixel.
- Temporal reprojection with YCoCg variance clipping based on the technique described by Salvi (GDC 2016) and Karis (SIGGRAPH 2014).
- Reinhard tone mapping with configurable exposure.
- Gamma correction via square root approximation.
- Firefly clamping using luminance-weighted SSE.
- AVX-accelerated accumulator presentation (8 pixels per iteration).

### Scene Representation

- Two-level sparse voxel grid (BrickMap). The outer grid is `GRIDSIZE^3` cells (default `WORLDSIZE=512`, `BRICKSIZE=8`, so `GRIDSIZE=64`). Each cell either has no brick or points to an 8x8x8 brick of packed `uint32_t` voxels.
- Dynamic brick pool that grows automatically when capacity is exceeded, safety-clamped to `GRIDSIZE^3`.
- Analytic sphere primitives with BVH acceleration (construction via tinyBVH, traversal via custom inlined code).
- Triangle mesh support via tinyBVH.

### Materials

- PBR material model with base color, metallic, roughness, anisotropy, index of refraction, transparency, and absorption density.
- Up to 256 materials managed by `MaterialManager`.
- Voxels pack an 8-bit material ID in the upper byte alongside 24-bit RGB color.
- Dielectric shading with stochastic reflect/refract selection and Beer-Lambert absorption.
- Anisotropic specular highlights using the Disney roughness-to-alpha convention.

### BRDF

- Cook-Torrance specular: anisotropic GGX NDF, Smith height-correlated geometry term, Schlick Fresnel approximation.
- Lambertian diffuse (energy-conserving division by pi).
- Fresnel-weighted energy conservation between diffuse and specular lobes.
- Roughness-aware Fresnel for environment map reflections.

### Lighting

- Directional lights with configurable angular radius for soft shadows.
- Point lights with spherical area light jitter.
- Spotlights with configurable cone angle, soft band falloff, pitch/yaw orientation, and area light radius.
- Shadow rays with transmittance accumulation through transparent voxels (Beer-Lambert).
- HDR environment map (skydome) importance sampling using marginal/conditional CDF inversion, as described in PBRT 3rd edition, Section 13.6.7.
- IBL contribution with Russian roulette probability gating.

### Camera

- Perspective camera with configurable field of view.
- Thin lens depth of field with concentric disk sampling.
- WASD + Space/Ctrl movement, IJKL look controls.
- Camera state persistence to `camera.bin`.

### User Interface

- ImGui-based editor windows for lights and materials.
- Material presets (matte, plastic, ceramic, rubber, chrome, gold, copper, brushed metal, glass, water, diamond, frosted).
- Real-time diagnostics overlay showing frame time, FPS, MRays/s, camera state, and scene statistics.
- HDR environment map selection from the `assets/textures/` directory.

### Scene Loading

- Procedural Perlin noise terrain generation.
- Compressed `.bin` scene file loading via zlib.

---

## Architecture

The project is split into three layers:

```
Source/
  ThirdParty/       -- Tmpl8 template layer, ImGui, tinyBVH
  Engine/            -- Core rendering engine (static library)
  Applications/      -- Executable applications (e.g. Demo)
```

The engine is built as a static library (`engine`) that applications link against. Each application is a separate CMake target.

### Key Classes

- `rt::core::Renderer` -- Owns the scene, camera, materials, lights, skydome, and accumulator. Entry point for frame rendering.
- `rt::core::Camera` -- Perspective camera with thin lens DoF and keyboard input handling.
- `rt::core::SkyDome` -- Loads equirectangular HDR files via stb_image. Provides importance sampling with marginal/conditional CDFs.
- `rt::scene::Scene` -- Thin orchestrator that owns the voxel world (via `BrickPool`), mesh list, and `AnalyticScene`. Exposes `findNearest`, `isOccluded`, and `transmittance` ray queries.
- `rt::scene::BrickPool` -- Owns the `BrickMap` heap allocation and all voxel read/write operations.
- `rt::scene::internal::BrickTraverser` -- Stateless free functions for brick-level DDA ray queries (nearest, occluded, transmittance).
- `rt::primitives::AnalyticScene` -- Manages analytic spheres with a tinyBVH-built BVH and custom inlined traversal.
- `rt::rendering::MaterialManager` -- Manages up to 256 PBR materials with registration, lookup, and update.
- `rt::management::LightManager` -- Stores and evaluates all light types. Produces `LightSample` structs consumed by the BRDF evaluation.
- `rt::management::UIManager` -- Coordinates the ImGui-based light editor, material editor, and diagnostics windows.

### Ray Query Pipeline

1. `Scene::findNearest` nudges the ray origin, sets up the outer DDA over the brick grid, and steps through cells. For each occupied cell, the inner DDA in `BrickTraverser::Nearest` walks the 8x8x8 voxels. After the voxel pass, mesh BVHs and the analytic scene are tested. The closest hit wins.
2. `Scene::isOccluded` follows the same structure but early-exits on any solid hit (shadow rays).
3. `Scene::transmittance` accumulates Beer-Lambert absorption through transparent voxels and analytic spheres.

### Shading Pipeline

`shadeHit` dispatches based on the voxel sentinel value:
- `0x40000000` indicates an analytic primitive hit, dispatched to either `shadeDielectric` (transparent) or `shadeOpaqueSphere` (opaque).
- All other nonzero values are treated as voxel hits, dispatched to `shadeVoxel`.

Each shading function returns a `BounceResult` containing the direct lighting contribution, a throughput scale factor, and a continuation flag. The iterative trace loop in `Renderer::trace` accumulates color and updates throughput across bounces.

---

## Requirements

- C++17 compiler.
- CMake 3.24 or later.
- OpenMP (found via `find_package(OpenMP REQUIRED)`).
- One of the supported compiler toolchains (see CMake presets below).

### Third-Party Dependencies (included in `Source/ThirdParty/`)

- **Tmpl8** -- Template layer providing windowing (GLFW), OpenGL context, math types (`float3`, `mat4`), and utility functions.
- **ImGui** -- Immediate-mode GUI library, configured with GLFW and OpenGL3 backends.
- **tinyBVH** -- Header-only BVH construction and traversal library.
- **stb_image** -- Header-only image loading (used by `SkyDome` for HDR files).
- **zlib** -- Compression library (used by `FileSceneLoader` for `.bin` scene files).

---

## Building

The project provides CMake presets for four compiler toolchains. Each preset supports Debug, Release, and RelWithDebInfo configurations.

### Configure

```bash
cmake --preset <preset-name>
```

Available configure presets:

| Preset      | Compiler  | Generator                  | Build Directory        |
|-------------|-----------|----------------------------|------------------------|
| `msvc`      | MSVC (cl) | Visual Studio 17 2022      | `build/msvc`           |
| `clang-cl`  | Clang-cl  | Visual Studio 17 2022      | `build/clang-cl`       |
| `gcc`       | GCC (g++) | Ninja Multi-Config         | `build/gcc`            |
| `clang`     | Clang++   | Ninja Multi-Config         | `build/clang`          |

### Build

```bash
cmake --build --preset <build-preset>
```

Available build presets follow the pattern `<toolchain>-<config>`, for example:

- `msvc-debug`, `msvc-release`, `msvc-dev`
- `gcc-debug`, `gcc-release`, `gcc-dev`
- `clang-debug`, `clang-release`, `clang-dev`
- `clang-cl-debug`, `clang-cl-release`, `clang-cl-dev`

The `dev` presets correspond to the `RelWithDebInfo` configuration.

### Application Targets

Individual applications can be enabled via CMake options. The following options are available:

- `VOXPOPULI_BUILD_DEMO`
- `VOXPOPULI_BUILD_LIGHTING`
- `VOXPOPULI_BUILD_REFLECTIONS`
- `VOXPOPULI_BUILD_STOCHASTIC`
- `VOXPOPULI_BUILD_PROJECTION`
- `VOXPOPULI_BUILD_PRIMITIVES`

Set `VOXPOPULI_BUILD_ALL=ON` to build all applications.

### Compiler Flags (MSVC)

In MSVC builds, the engine is compiled with `/W4 /WX /permissive- /EHsc /MP /arch:AVX2`. Release builds additionally enable `/O2 /Ob2 /Oi /Ot /GL`.

---

## Usage

### Controls

| Key              | Action              |
|------------------|----------------------|
| W / A / S / D    | Move forward / left / backward / right |
| Space            | Move up              |
| Left Ctrl        | Move down            |
| I / K            | Look up / down       |
| J / L            | Look left / right    |

### UI Windows

The ImGui menu bar provides toggles for:

- **Diagnostics** -- Frame time, FPS, MRays/s, camera position and direction, light and material counts.
- **Light Editor** -- Add, remove, and configure directional, point, and spot lights.
- **Material Editor** -- Select and edit PBR material properties with real-time preview and preset buttons.

### HDR Environment Maps

Place `.hdr` files in the `assets/textures/` directory. The application scans this directory on startup and presents a dropdown in the UI for selection. The default file is `minedump_flats_4k.hdr`.

---

## Project Structure

```
voxpopuli/
  CMakeLists.txt              -- Root CMake configuration
  CMakePresets.json           -- Compiler and build presets
  .clang-tidy                 -- Static analysis and naming convention rules
  Source/
    ThirdParty/
      Tmpl8/                  -- Template layer (windowing, math, OpenGL)
      imgui/                  -- ImGui with GLFW/OpenGL3 backends
      tinybvh/                -- Header-only BVH library
    Engine/
      Public/rt/
        core/                 -- Camera, Ray, Renderer, SkyDome
        brdf/                 -- BRDF types, Fresnel, NDF, geometry, diffuse, evaluation
        scene/                -- Scene, BrickMap, BrickPool, SceneLoader
        rendering/            -- Material, MaterialManager, MaterialEditor, BitPacker, Mesh
        rendering/primitives/ -- AnalyticScene, Sphere, HitInfo
        lights/               -- DirectionalLight, PointLight, SpotLight, LightSample
        management/           -- LightManager, LightEditor, UIManager
      Private/
        scene/                -- DDA, BrickTraverser, CubeHelpers (internal)
    Applications/
      Demo/                   -- Demo application (DemoApplication, SceneBuilder)
```

---

## Coding Conventions

The project uses a `.clang-tidy` configuration that enforces `readability-identifier-naming` with the following conventions:

- **Classes and structs**: `CamelCase`
- **Methods and free functions**: `CamelCase`
- **Class members**: `camelBack` with `m_` prefix and Hungarian type prefix enabled
- **Local variables and parameters**: `camelBack` with Hungarian type prefix enabled
- **Global variables**: `camelBack` with `g_` prefix
- **Global constants**: `UPPER_CASE` with `g_k` prefix
- **Static variables**: `camelBack` with `s_` prefix
- **Static constants**: `UPPER_CASE` with `s_k` prefix
- **Enums and enum constants**: `CamelCase`
- **Macros**: `UPPER_CASE`
- **Constexpr variables**: `UPPER_CASE`
- **Namespaces**: `CamelCase`

---

## References

The following references are cited in the source code and inform the implementation:

- Burley, B. (2012). "Physically-Based Shading at Disney." SIGGRAPH Course Notes.
- Karis, B. (2013). "Real Shading in Unreal Engine 4." SIGGRAPH Course Notes.
- Karis, B. (2014). "High Quality Temporal Supersampling." SIGGRAPH.
- Salvi, M. (2016). "An Excursion in Temporal Supersampling." GDC.
- Pharr, M., Jakob, W., Humphreys, G. (2016). "Physically Based Rendering: From Theory to Implementation," 3rd edition. Sections 12.6, 13.6.7, 14.2.4.
- Schlick, C. (1994). "An Inexpensive BRDF Model for Physically-based Rendering." Computer Graphics Forum.
- glTF 2.0 Specification -- PBR metallic-roughness material model.
- Jacco Bikker, "How to Build a BVH" series (2022), https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/
- Jacco Bikker, "Ray Tracing with Voxels" series (2022), https://jacco.ompf2.com/2024/04/24/ray-tracing-with-voxels-in-c-series-part-1/
- University of Oregon PBR lecture slides, https://ix.cs.uoregon.edu/~hank/441/lectures/pbr_slides.pdf


## License

This project is dedicated to the public domain under the CC0 1.0 Universal license. You may copy, modify, distribute, and use the work, including for commercial purposes, without asking permission.See the LICENSE file for the full legal text.  

Note that third-party dependencies included under Source/ThirdParty/ retain their own licenses: ImGui (MIT), tinyBVH (MIT), stb_image (MIT / public domain), and zlib (zlib license).
