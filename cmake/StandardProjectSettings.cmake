# ==============================================================================
# StandardProjectSettings.cmake
#
# Project-wide CMake configuration.
# ==============================================================================
#
# Responsibilities:
#   - Configure the C++ language standard
#   - Disable compiler-specific language extensions
#   - Export compile_commands.json
#   - Set a default build type
#
# ==============================================================================

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for clangd, clang-tidy, VS Code, etc.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build Type      : ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard    : ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler        : ${CMAKE_CXX_COMPILER_ID}")
