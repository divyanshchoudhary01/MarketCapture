# ==============================================================================
# StaticAnalyzers.cmake
#
# Integrates static analysis tools.
#
# Current:
#   - clang-tidy
#
# Future:
#   - cppcheck
#   - include-what-you-use
# ==============================================================================

find_program(CLANG_TIDY_EXE NAMES clang-tidy)

if(CLANG_TIDY_EXE)

    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")

    set(CMAKE_CXX_CLANG_TIDY
        ${CLANG_TIDY_EXE}
    )

else()

    message(STATUS "clang-tidy not found")

endif()

