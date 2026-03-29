message(STATUS "[Engine] Configuring Engine library...")

# ============================================================
#  Engine - Raytracing Engine (Static Library)
# ============================================================
add_library(engine STATIC)

set_target_properties(engine PROPERTIES
    FOLDER "Libraries"
)

# ============================================================
#  Private source files
#  All .cpp files live under Private/
# ============================================================
target_sources(engine PRIVATE
    # Root
    Private/core/camera.cpp
    Private/core/ray.cpp
    Private/core/renderer.cpp
    Private/core/sky_dome.cpp

    # Scene
    Private/scene/scene.cpp
    Private/scene/scene_loader.cpp
    Private/scene/brick_pool.cpp
    Private/scene/brick_traverser.cpp

    # Rendering
    Private/rendering/material_manager.cpp
    Private/rendering/material_editor.cpp
    Private/rendering/analytic_scene.cpp
    Private/rendering/shading.cpp

    # Management
    Private/management/light_manager.cpp
    Private/management/light_editor.cpp
    Private/management/ui_manager.cpp
    Private/management/renderer_panel.cpp
    Private/management/viewport_panel.cpp
)

# ============================================================
#  Public headers
#  Exposed to consumers — live under include/Public/rt/
# ============================================================
target_sources(engine PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Public"
    FILES
    # Root
    Public/rt/core/camera.h
    Public/rt/core/ray.h
    Public/rt/core/renderer.h
    Public/rt/core/sky_dome.h
    Public/rt/core/blue_noise.h

    # Scene
    Public/rt/scene/scene.h
    Public/rt/scene/scene_loader.h
    Public/rt/scene/brick_map.h
    Public/rt/scene/brick_pool.h

    # BRDF
    Public/rt/brdf/brdf.h
    Public/rt/brdf/brdf_types.h
    Public/rt/brdf/brdf_aniso.h
    Public/rt/brdf/brdf_diffuse.h
    Public/rt/brdf/brdf_evaluate.h
    Public/rt/brdf/brdf_fresnel.h
    Public/rt/brdf/brdf_geometry.h
    Public/rt/brdf/brdf_ndf.h
    Public/rt/brdf/bsdf_types.h
    Public/rt/brdf/bsdf.h

    # Rendering
    Public/rt/rendering/material.h
    Public/rt/rendering/material_manager.h
    Public/rt/rendering/material_editor.h
    Public/rt/rendering/bit_packer.h
    Public/rt/rendering/mesh.h
    Public/rt/rendering/config.h
    Public/rt/rendering/simd_utils.h
    Public/rt/rendering/shading.h

    # Rendering - Primitives
    Public/rt/rendering/primitives/hit_info.h
    Public/rt/rendering/primitives/sphere.h
    Public/rt/rendering/primitives/voxel_instance.h
    Public/rt/rendering/primitives/analytic_scene.h

    # Lights
    Public/rt/lights/light_sample.h
    Public/rt/lights/point_light.h
    Public/rt/lights/directional_light.h
    Public/rt/lights/spotlight.h

    # Management
    Public/rt/management/light_manager.h
    Public/rt/management/light_editor.h
    Public/rt/management/ui_manager.h
    Public/rt/management/renderer_panel.h
    Public/rt/management/viewport_panel.h
)

# ============================================================
#  Private headers
#  Internal only — live under include/Private/
# ============================================================
target_sources(engine PRIVATE
    Private/scene/dda.h
    Private/scene/brick_traverser.h
    Private/scene/cube_helpers.h
)

# ============================================================
#  Include directories
# ============================================================

# Public: consumers get the rt/ folder on their include path,
# so they write: #include "rt/camera.h"
target_include_directories(engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
    "${CMAKE_SOURCE_DIR}/lib/tinybvh"
)

# Private: the engine's own .cpp files can reach private headers
target_include_directories(engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Private"
    "${SOURCE_THIRDPARTY_DIR}/imgui"
    "${SOURCE_THIRDPARTY_DIR}/OpenCL/inc"
    "${SOURCE_THIRDPARTY_DIR}/glad"
    "${SOURCE_THIRDPARTY_DIR}/glfw/include"
    "${SOURCE_THIRDPARTY_DIR}/zlib"
)

# ============================================================
#  Link libraries
# ============================================================
target_link_libraries(engine PUBLIC
    tmpl8
    tinybvh
    miniaudio
    OpenMP::OpenMP_CXX
)

# ============================================================
#  Compiler flags
# ============================================================
if(MSVC)
    target_compile_options(engine PRIVATE
        /W4
        /WX
        /permissive-
        /EHsc
        /MP
        /arch:AVX2
        $<$<CONFIG:Debug>:/Zi /Od /RTC1 /MDd>
        $<$<CONFIG:Release>:/O2 /Ob2 /Oi /Ot /GL /MD>
        $<$<CONFIG:RelWithDebInfo>:/O2 /Ob2 /Oi /Ot /Zi /MD>
    )
endif()

# ============================================================
#  Preprocessor definitions
# ============================================================
target_compile_definitions(engine PRIVATE
    _CRT_SECURE_NO_DEPRECATE
    _WINDOWS
    $<$<CONFIG:Debug>:WIN32 _DEBUG>
    $<$<CONFIG:Release>:WIN64 NDEBUG>
    $<$<CONFIG:RelWithDebInfo>:WIN64 NDEBUG>
)

message(STATUS "[Engine] Engine library configured")