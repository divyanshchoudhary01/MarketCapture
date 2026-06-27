# ==============================================================================
# Sanitizers.cmake
#
# Enables runtime sanitizers for Debug builds.
#
# Supported:
#   - AddressSanitizer
#   - UndefinedBehaviorSanitizer
# ==============================================================================

function(enable_sanitizers target)

    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

        target_compile_options(${target}
            PRIVATE
            -fsanitize=address
            -fsanitize=undefined
            -fno-omit-frame-pointer
        )

        target_link_options(${target}
            PRIVATE
            -fsanitize=address
            -fsanitize=undefined
        )

    endif()

endfunction()
