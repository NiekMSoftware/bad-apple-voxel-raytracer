message(STATUS "[Applications] Configuring applications...")

# ============================================================
#  Build options
#  Each application can be toggled individually, or all at
#  once via VOXPOPULI_BUILD_ALL. These are cache variables so
#  they appear in cmake-gui and can be set via -D on the
#  command line, e.g: cmake -DVOXPOPULI_BUILD_ALL=ON
#
#  CMake reference:
#    https://cmake.org/cmake/help/latest/command/option.html
# ============================================================
option(VOXPOPULI_BUILD_ALL          "Build all applications"        OFF)
option(VOXPOPULI_BUILD_DEMO         "Build Demo application"        ON)
option(VOXPOPULI_BUILD_LIGHTING     "Build Lighting playground"     OFF)
option(VOXPOPULI_BUILD_REFLECTIONS  "Build Reflections playground"  OFF)
option(VOXPOPULI_BUILD_STOCHASTIC   "Build Stochastic playground"   OFF)
option(VOXPOPULI_BUILD_PROJECTION   "Build Projection playground"   OFF)
option(VOXPOPULI_BUILD_PRIMITIVES   "Build Primitives playground"   OFF)

# ============================================================
#  Applications
# ============================================================
if(VOXPOPULI_BUILD_ALL OR VOXPOPULI_BUILD_DEMO)
    add_subdirectory(Demo)
endif()

if(VOXPOPULI_BUILD_ALL OR VOXPOPULI_BUILD_LIGHTING)
    add_subdirectory(Playground/Lighting)
endif()

if(VOXPOPULI_BUILD_ALL OR VOXPOPULI_BUILD_REFLECTIONS)
    add_subdirectory(Playground/Reflections)
endif()

if(VOXPOPULI_BUILD_ALL OR VOXPOPULI_BUILD_STOCHASTIC)
    add_subdirectory(Playground/Stochastic)
endif()

if(VOXPOPULI_BUILD_ALL OR VOXPOPULI_BUILD_PROJECTION)
    add_subdirectory(Playground/Projection)
endif()

if(VOXPOPULI_BUILD_ALL OR VOXPOPULI_BUILD_PRIMITIVES)
    add_subdirectory(Playground/Primitives)
endif()

message(STATUS "[Applications] Applications configured")