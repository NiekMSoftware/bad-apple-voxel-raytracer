# ============================================================================
# copy_assets.cmake - Per-application post-build asset copy event
# ============================================================================

# Registers a POST_BUILD event on the given target that copies the root
# assets/ folder next to the target's executable after every successful build.
#
# Usage (called automatically by create_app):
#   copy_assets_to_output(<target>)
#
# CMake reference:
#   https://cmake.org/cmake/help/latest/command/add_custom_command.html#build-events

function(copy_assets_to_output target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${target}>/assets"
        COMMENT "[${target}] Copying assets to $<TARGET_FILE_DIR:${target}>/assets"
    )
endfunction()