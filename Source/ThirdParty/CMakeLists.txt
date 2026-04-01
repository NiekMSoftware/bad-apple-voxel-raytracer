message(STATUS "[ThirdParty] Configuring third-party libraries...")

# ============================================================
#  Tmpl8 — must be configured before ImGui because ImGui
#  sources include "template.h" from the Tmpl8 template layer
# ============================================================
add_subdirectory(Tmpl8)

# ============================================================
#  ImGui
# ============================================================
add_library(imgui STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp"
)

target_include_directories(imgui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
    "${SOURCE_THIRDPARTY_DIR}/glad/include"
    "${SOURCE_THIRDPARTY_DIR}/glfw/include"
)

# ImGui sources open with #include "template.h" from Tmpl8.
# Linking tmpl8 here propagates its include directories to imgui,
# which makes "template.h" visible to the imgui compiler invocation.
target_link_libraries(imgui PUBLIC tmpl8)

set_target_properties(imgui PROPERTIES FOLDER "ThirdParty")

# ============================================================
#  TinyBVH — header-only library
#  Declared as INTERFACE so consumers get the include path
#  transitively without compiling any sources themselves.
# ============================================================
add_library(tinybvh INTERFACE)

target_include_directories(tinybvh INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/tinybvh"
)

set_target_properties(tinybvh PROPERTIES FOLDER "ThirdParty")

# Disable everything we don't need
set(MINIAUDIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_BUILD_TOOLS    OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_INSTALL        OFF CACHE BOOL "" FORCE)

set(CMAKE_FOLDER "ThirdParty")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/miniaudio")
unset(CMAKE_FOLDER)

message(STATUS "[ThirdParty] Third-party libraries configured")