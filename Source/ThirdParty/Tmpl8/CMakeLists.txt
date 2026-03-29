message(STATUS "[Tmpl8] Configuring Tmpl8 library...")

# ============================================================
#  Tmpl8 - Template Engine Library (Static Library)
# ============================================================
enable_language(C CXX)
add_library(tmpl8 STATIC)

set_target_properties(tmpl8 PROPERTIES
    FOLDER "Libraries"
)

# ============================================================
#  Source files
# ============================================================

# glad.c must be compiled as C, not C++
set_source_files_properties(
    "${SOURCE_THIRDPARTY_DIR}/glad/src/glad.c"
    PROPERTIES LANGUAGE C
)

target_sources(tmpl8 PRIVATE
    # Template sources
    tmpl8/tmpl8math.cpp
    tmpl8/surface.cpp
    tmpl8/opengl.cpp
    tmpl8/opencl.cpp
    tmpl8/app.cpp
    tmpl8/template.cpp

    # Glad
    "${SOURCE_THIRDPARTY_DIR}/glad/src/glad.c"
)

# ============================================================
#  Public headers
# ============================================================
target_sources(tmpl8 PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
    tmpl8/common.h
    tmpl8/opencl.h
    tmpl8/opengl.h
    tmpl8/template.h
    tmpl8/surface.h
    tmpl8/tmpl8math.h
    tmpl8/app.h
)

target_sources(tmpl8 PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${SOURCE_THIRDPARTY_DIR}/glad/include"
    FILES
    "${SOURCE_THIRDPARTY_DIR}/glad/include/glad.h"
    "${SOURCE_THIRDPARTY_DIR}/glad/include/khrplatform.h"
)

# ============================================================
#  Include directories
# ============================================================
target_include_directories(tmpl8 PUBLIC
    "${SOURCE_THIRDPARTY_DIR}/imgui"
    "${SOURCE_THIRDPARTY_DIR}/glad/include"
    "${SOURCE_THIRDPARTY_DIR}/glfw/include"
    "${SOURCE_THIRDPARTY_DIR}/OpenCL/inc"
    "${SOURCE_THIRDPARTY_DIR}/zlib"
    "${SOURCE_THIRDPARTY_DIR}"
)

target_include_directories(tmpl8 PRIVATE
    "${SOURCE_THIRDPARTY_DIR}"
    "${SOURCE_THIRDPARTY_DIR}/imgui"
    "${SOURCE_THIRDPARTY_DIR}/glad"
    "${SOURCE_THIRDPARTY_DIR}/glfw/include"
    "${SOURCE_THIRDPARTY_DIR}/OpenCL/inc"
    "${SOURCE_THIRDPARTY_DIR}/zlib"
)

# ============================================================
#  Link libraries
# ============================================================
target_link_libraries(tmpl8 PUBLIC imgui)

target_link_directories(tmpl8 PUBLIC
    "${SOURCE_THIRDPARTY_DIR}/glfw/lib-vc2022"
    "${SOURCE_THIRDPARTY_DIR}/zlib"
    "${SOURCE_THIRDPARTY_DIR}/OpenCL/lib"
)

target_link_libraries(tmpl8 PUBLIC
    glfw3.lib
    OpenCL.lib
    libz-static.lib
    opengl32.lib
)

# ============================================================
#  Compiler flags
# ============================================================
if(MSVC)
    target_compile_options(tmpl8 PRIVATE
        /W4
        /EHsc
        /MP
        $<$<CONFIG:Debug>:/Zi /Od /RTC1 /MDd>
        $<$<CONFIG:Release>:/O2 /Oi /Ot /arch:AVX2 /fp:fast /GS- /MD>
        $<$<CONFIG:RelWithDebInfo>:/O2 /Oi /Ot /arch:AVX2 /fp:fast /GS- /Zi /MD>
    )
endif()

# ============================================================
#  Preprocessor definitions
# ============================================================
target_compile_definitions(tmpl8 PRIVATE
    _CRT_SECURE_NO_DEPRECATE
    _WINDOWS
    $<$<CONFIG:Debug>:WIN32 _DEBUG>
    $<$<CONFIG:Release>:WIN64 NDEBUG>
    $<$<CONFIG:RelWithDebInfo>:WIN64 NDEBUG>
)

message(STATUS "[Tmpl8] Tmpl8 library configured")