# ==============================================================================
# CompilerWarnings.cmake
#
# Enables a consistent warning level across supported compilers.
# ==============================================================================

function(enable_compiler_warnings target)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

        target_compile_options(${target}
            PRIVATE

            -Wall
            -Wextra
            -Wpedantic

            -Wshadow
            -Wconversion
            -Wsign-conversion
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wnon-virtual-dtor
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2

        )

    elseif(MSVC)

        target_compile_options(${target}
            PRIVATE
            /W4
        )

    endif()

endfunction()
