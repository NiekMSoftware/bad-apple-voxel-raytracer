# ============================================================================
# create_app.cmake - Utility function for creating application targets
# ============================================================================

function(create_app)
    # ========================================================================
    # Parse arguments
    # ========================================================================
    set(options         RECURSIVE)
    set(oneValueArgs    NAME FOLDER SOURCE_DIR)
    set(multiValueArgs  SOURCES HEADERS LINK_LIBS ASSETS EXCLUDE_PATTERNS)
    cmake_parse_arguments(APP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT APP_NAME)
        message(FATAL_ERROR "create_app: NAME is required")
    endif()

    # ========================================================================
    # Collect source files
    # ========================================================================
    set(ALL_SOURCES "")
    set(ALL_HEADERS "")

    if(APP_SOURCE_DIR)
        set(SEARCH_DIR "${APP_SOURCE_DIR}")
    else()
        set(SEARCH_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    if(APP_RECURSIVE OR (NOT APP_SOURCES AND NOT APP_HEADERS))
        file(GLOB_RECURSE FOUND_SOURCES CONFIGURE_DEPENDS
            "${SEARCH_DIR}/*.cpp"
            "${SEARCH_DIR}/*.c"
            "${SEARCH_DIR}/*.cxx"
            "${SEARCH_DIR}/*.cc"
        )

        file(GLOB_RECURSE FOUND_HEADERS CONFIGURE_DEPENDS
            "${SEARCH_DIR}/*.h"
            "${SEARCH_DIR}/*.hpp"
            "${SEARCH_DIR}/*.hxx"
            "${SEARCH_DIR}/*.inl"
        )

        if(APP_EXCLUDE_PATTERNS)
            foreach(PATTERN ${APP_EXCLUDE_PATTERNS})
                list(FILTER FOUND_SOURCES EXCLUDE REGEX "${PATTERN}")
                list(FILTER FOUND_HEADERS EXCLUDE REGEX "${PATTERN}")
            endforeach()
        endif()

        list(APPEND ALL_SOURCES ${FOUND_SOURCES})
        list(APPEND ALL_HEADERS ${FOUND_HEADERS})

        list(LENGTH ALL_SOURCES SOURCE_COUNT)
        list(LENGTH ALL_HEADERS HEADER_COUNT)
        message(STATUS "${APP_NAME}: Found ${SOURCE_COUNT} source files and ${HEADER_COUNT} headers recursively")
    endif()

    if(APP_SOURCES)
        list(APPEND ALL_SOURCES ${APP_SOURCES})
    endif()

    if(APP_HEADERS)
        list(APPEND ALL_HEADERS ${APP_HEADERS})
    endif()

    list(REMOVE_DUPLICATES ALL_SOURCES)
    list(REMOVE_DUPLICATES ALL_HEADERS)

    if(NOT ALL_SOURCES)
        message(FATAL_ERROR "create_app: No source files found for ${APP_NAME}")
    endif()

    # ========================================================================
    # Create the executable
    # ========================================================================
    add_executable(${APP_NAME})

    target_sources(${APP_NAME} PRIVATE ${ALL_SOURCES})

    if(ALL_HEADERS)
        target_sources(${APP_NAME} PRIVATE ${ALL_HEADERS})
    endif()

    # ========================================================================
    # IDE source groups
    # ========================================================================
    source_group(TREE "${SEARCH_DIR}" PREFIX "Source Files" FILES ${ALL_SOURCES})
    source_group(TREE "${SEARCH_DIR}" PREFIX "Header Files" FILES ${ALL_HEADERS})

    # ========================================================================
    # Output directories
    # ========================================================================
    set_target_properties(${APP_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY                "${CMAKE_SOURCE_DIR}/x64/${APP_NAME}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_SOURCE_DIR}/x64/${APP_NAME}/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_SOURCE_DIR}/x64/${APP_NAME}/Release"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/x64/${APP_NAME}/RelWithDebInfo"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_SOURCE_DIR}/x64/${APP_NAME}/MinSizeRel"
        DEBUG_POSTFIX "_debug"
        VS_DEBUGGER_WORKING_DIRECTORY           "$<TARGET_FILE_DIR:${APP_NAME}>"
    )

    if(APP_FOLDER)
        set_target_properties(${APP_NAME} PROPERTIES FOLDER "${APP_FOLDER}")
    else()
        set_target_properties(${APP_NAME} PROPERTIES FOLDER "Applications")
    endif()

    # ========================================================================
    # Include directories
    # ========================================================================
    set(INCLUDE_DIRS "${SEARCH_DIR}")
    foreach(HEADER ${ALL_HEADERS})
        get_filename_component(HEADER_DIR "${HEADER}" DIRECTORY)
        list(APPEND INCLUDE_DIRS "${HEADER_DIR}")
    endforeach()
    list(REMOVE_DUPLICATES INCLUDE_DIRS)

    target_include_directories(${APP_NAME} PRIVATE ${INCLUDE_DIRS})

    # ========================================================================
    # Link libraries
    # ========================================================================

    # engine transitively brings in tmpl8, tinybvh, imgui, and OpenMP
    target_link_libraries(${APP_NAME} PRIVATE engine)

    if(APP_LINK_LIBS)
        target_link_libraries(${APP_NAME} PRIVATE ${APP_LINK_LIBS})
    endif()

    # Windows system libraries
    target_link_libraries(${APP_NAME} PRIVATE
        winmm.lib
        advapi32.lib
        user32.lib
        gdi32.lib
        shell32.lib
        OpenGL32.lib
    )

    # Third-party library directories
    target_link_directories(${APP_NAME} PRIVATE
        "${SOURCE_THIRDPARTY_DIR}/glfw/lib-vc2022"
        "${SOURCE_THIRDPARTY_DIR}/zlib"
        "${SOURCE_THIRDPARTY_DIR}/OpenCL/lib"
    )

    target_link_libraries(${APP_NAME} PRIVATE
        glfw3.lib
        OpenCL.lib
        libz-static.lib
    )

    # ========================================================================
    # Compiler flags (MSVC)
    # ========================================================================
    if(MSVC)
        target_compile_options(${APP_NAME} PRIVATE
            /W4
            /EHsc
            /MP
            # /arch:AVX2 is set for all configs so TinyBVH's AVX/AVX2 compile-time
            # checks pass and the associated warnings are suppressed
            /arch:AVX2
            $<$<CONFIG:Debug>:/Zi /Od /RTC1 /MDd>
            $<$<CONFIG:Release>:/O2 /Oi /Ot /fp:fast /GS- /MD>
            $<$<CONFIG:RelWithDebInfo>:/O2 /Oi /Ot /fp:fast /GS- /Zi /MD>
        )

        target_link_options(${APP_NAME} PRIVATE
            /SUBSYSTEM:CONSOLE
            /DYNAMICBASE:NO
            # Suppress LNK4098: in Debug, explicitly exclude the release CRT so
            # the linker only sees msvcrtd.lib and not the conflicting msvcrt.lib
            $<$<CONFIG:Debug>:/NODEFAULTLIB:msvcrt.lib>
            $<$<CONFIG:Release>:/OPT:REF /OPT:ICF /INCREMENTAL:NO>
            $<$<CONFIG:RelWithDebInfo>:/OPT:REF /OPT:ICF /INCREMENTAL:NO /DEBUG>
        )
    endif()

    # ========================================================================
    # Preprocessor definitions
    # ========================================================================
    target_compile_definitions(${APP_NAME} PRIVATE
        _CRT_SECURE_NO_DEPRECATE
        _WINDOWS
        $<$<CONFIG:Debug>:WIN32 _DEBUG>
        $<$<CONFIG:Release>:WIN64 NDEBUG>
        $<$<CONFIG:RelWithDebInfo>:WIN64 NDEBUG>
    )


    # ========================================================================
    # Post-build events
    # ========================================================================
    copy_assets_to_output(${APP_NAME})

    message(STATUS "Created application: ${APP_NAME}")

endfunction()