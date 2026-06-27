cmake_minimum_required(VERSION 3.25)

project(
    MarketCapture
    VERSION 0.1.0
    DESCRIPTION "Production-grade low-latency market data platform"
    LANGUAGES CXX
)

option(ENABLE_CLANG_TIDY "Enable clang-tidy analysis" ON)

if(ENABLE_CLANG_TIDY)

    find_program(CLANG_TIDY_EXE NAMES clang-tidy)

    if(CLANG_TIDY_EXE)

        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")

        set(CMAKE_CXX_CLANG_TIDY
            ${CLANG_TIDY_EXE}
        )

    endif()

endif()

find_package(Doxygen)

if(DOXYGEN_FOUND)

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation"
    )

endif()

include(CTest)

if(BUILD_TESTING)
    enable_testing()
endif()
# ------------------------------------------------------------------------------
# CMake Modules
# ------------------------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH
    "${CMAKE_SOURCE_DIR}/cmake"
)

include(StandardProjectSettings)
include(CompilerWarnings)
include(Sanitizers)
include(StaticAnalyzers)

# ------------------------------------------------------------------------------
# Executable
# ------------------------------------------------------------------------------

add_executable(MarketCapture
    src/main.cpp
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_subdirectory(benchmarks)
# ------------------------------------------------------------------------------
# Include Directories
# ------------------------------------------------------------------------------

target_include_directories(MarketCapture
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

# ------------------------------------------------------------------------------
# Compiler Configuration
# ------------------------------------------------------------------------------

enable_compiler_warnings(MarketCapture)
enable_sanitizers(MarketCapture)

