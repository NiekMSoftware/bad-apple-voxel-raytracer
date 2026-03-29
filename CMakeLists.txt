cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

message(STATUS "----------------------------------------------")
message(STATUS "    CMake version  : ${CMAKE_VERSION}")
message(STATUS "    CMake generator: ${CMAKE_GENERATOR}")
message(STATUS "----------------------------------------------")

# =================================================================
#  Project
# =================================================================
project(voxpopuli
  VERSION     0.3.5  
  DESCRIPTION "Voxel-based CPU ray tracer"
  LANGUAGES   C CXX
)

message(STATUS "[${PROJECT_NAME}] v${PROJECT_VERSION} - ${PROJECT_DESCRIPTION}")
message(STATUS "[${PROJECT_NAME}] Source dir : ${PROJECT_SOURCE_DIR}")
message(STATUS "[${PROJECT_NAME}] Binary dir : ${PROJECT_BINARY_DIR}")

# ============================================================
#  Guard: prevent in-source builds
# ============================================================
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "[${PROJECT_NAME}] In-source builds are not allowed.\n"
    "        Use a build preset or create a separate build directory.\n"
    "        Remove CMakeCache.txt and the CMakeFiles/ directory, then try again."
  )
endif()

# ============================================================
#  Global CMake settings
# ============================================================
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")

if(CMAKE_CONFIGURATION_TYPES)
    message(STATUS "[${PROJECT_NAME}] Output dir : ${CMAKE_BINARY_DIR}/<Debug|Release|RelWithDebInfo>")
else()
    message(STATUS "[${PROJECT_NAME}] Output dir : ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
endif()

# ============================================================
#  CMake modules
# ============================================================
message(STATUS "[${PROJECT_NAME}] Loading cmake modules...")

include(cmake/copy_assets.cmake)
include(cmake/create_app.cmake)

message(STATUS "[${PROJECT_NAME}] cmake modules loaded")

# ============================================================
#  External dependencies
# ============================================================
find_package(OpenMP REQUIRED)

# ============================================================
#  Platform detection
# ============================================================
message(STATUS "[${PROJECT_NAME}] Detecting platform...")

if(WIN32)
  message(STATUS "[${PROJECT_NAME}] Platform : Windows")
elseif(UNIX AND NOT APPLE)
  message(STATUS "[${PROJECT_NAME}] Platform : Linux")
else()
  message(WARNING "[${PROJECT_NAME}] Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

message(STATUS "[${PROJECT_NAME}] System    : ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "[${PROJECT_NAME}] Compiler  : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "[${PROJECT_NAME}] Toolchain : ${CMAKE_CXX_COMPILER}")

# ============================================================
#  Source directories
# ============================================================
set(SOURCE_THIRDPARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/Source/ThirdParty")
set(SOURCE_ENGINE_DIR     "${CMAKE_CURRENT_LIST_DIR}/Source/Engine")
set(SOURCE_APPS_DIR       "${CMAKE_CURRENT_LIST_DIR}/Source/Applications")

message(STATUS "[${PROJECT_NAME}] ThirdParty : ${SOURCE_THIRDPARTY_DIR}")
message(STATUS "[${PROJECT_NAME}] Engine     : ${SOURCE_ENGINE_DIR}")
message(STATUS "[${PROJECT_NAME}] Apps       : ${SOURCE_APPS_DIR}")

# ============================================================
#  Source tree
# ============================================================
message(STATUS "[${PROJECT_NAME}] Configuring source tree...")

add_subdirectory(${SOURCE_THIRDPARTY_DIR})
add_subdirectory(${SOURCE_ENGINE_DIR})
add_subdirectory(${SOURCE_APPS_DIR})

message(STATUS "[${PROJECT_NAME}] Source tree configured")

# ============================================================
#  Summary
#
#  App names and their corresponding option variables are
#  defined as paired lists and iterated together using
#  cmake_language(CALL) with list(GET ...) by index.
#  This means adding a new app only requires adding one entry
#  to each list — the summary updates automatically.
# ============================================================

# Paired lists — index N in APP_NAMES matches index N in APP_OPTIONS
set(APP_NAMES
    "Demo"
    "Lighting"
    "Reflections"
    "Stochastic"
    "Projection"
    "Primitives"
)
set(APP_OPTIONS
    "VOXPOPULI_BUILD_DEMO"
    "VOXPOPULI_BUILD_LIGHTING"
    "VOXPOPULI_BUILD_REFLECTIONS"
    "VOXPOPULI_BUILD_STOCHASTIC"
    "VOXPOPULI_BUILD_PROJECTION"
    "VOXPOPULI_BUILD_PRIMITIVES"
)

message(STATUS "--------------------------------------------")
message(STATUS "  ${PROJECT_NAME} v${PROJECT_VERSION}")
if(CMAKE_CONFIGURATION_TYPES)
    message(STATUS "  Configs    : ${CMAKE_CONFIGURATION_TYPES}")
else()
    message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
endif()
message(STATUS "  Compiler   : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C++ std    : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Platform   : ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "  Generator  : ${CMAKE_GENERATOR}")
if(CMAKE_CONFIGURATION_TYPES)
    message(STATUS "  Output     : ${CMAKE_BINARY_DIR}/<Debug|Release|RelWithDebInfo>")
else()
    message(STATUS "  Output     : ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
endif()
message(STATUS "  ---")
message(STATUS "  Applications")

list(LENGTH APP_NAMES APP_COUNT)
math(EXPR APP_LAST "${APP_COUNT} - 1")

foreach(i RANGE ${APP_LAST})
    list(GET APP_NAMES   ${i} APP_NAME)
    list(GET APP_OPTIONS ${i} APP_OPTION)

    if(${APP_OPTION} OR VOXPOPULI_BUILD_ALL)
        message(STATUS "    ${APP_NAME} : ON")
    else()
        message(STATUS "    ${APP_NAME} : OFF")
    endif()
endforeach()

message(STATUS "--------------------------------------------")