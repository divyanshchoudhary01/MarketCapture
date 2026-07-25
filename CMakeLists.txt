cmake_minimum_required(VERSION 3.20)
project(MarketCapture VERSION 1.0.0 LANGUAGES CXX)

option(MARKETCAPTURE_BUILD_TESTS "Build unit tests" ON)
option(MARKETCAPTURE_BUILD_EXAMPLES "Build examples" ON)
option(MARKETCAPTURE_BUILD_BENCHMARKS "Build performance benchmark" ON)

add_library(marketcapture
    src/itch.cpp
    src/config.cpp
    src/moldudp64.cpp
    src/pipeline.cpp
    src/order_book.cpp
    src/recorder.cpp
    src/live_feed.cpp
)
add_library(MarketCapture::marketcapture ALIAS marketcapture)
target_include_directories(marketcapture PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(marketcapture PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(marketcapture PRIVATE /W4 /permissive-)
else()
    target_compile_options(marketcapture PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(WIN32)
    target_link_libraries(marketcapture PUBLIC ws2_32)
endif()

if(MARKETCAPTURE_BUILD_EXAMPLES)
    add_executable(marketcapture_demo examples/demo.cpp)
    target_link_libraries(marketcapture_demo PRIVATE MarketCapture::marketcapture)

    add_executable(marketcapture_live examples/live_capture.cpp)
    find_package(Threads REQUIRED)
    target_link_libraries(marketcapture_live PRIVATE MarketCapture::marketcapture Threads::Threads)
endif()

if(MARKETCAPTURE_BUILD_BENCHMARKS)
    add_executable(marketcapture_benchmark benchmarks/benchmark.cpp)
    target_link_libraries(marketcapture_benchmark PRIVATE MarketCapture::marketcapture)
endif()

if(MARKETCAPTURE_BUILD_TESTS)
    enable_testing()
    add_executable(marketcapture_tests tests/tests.cpp)
    target_link_libraries(marketcapture_tests PRIVATE MarketCapture::marketcapture)
    add_test(NAME marketcapture_tests COMMAND marketcapture_tests)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        get_filename_component(MINGW_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        set_tests_properties(marketcapture_tests PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${MINGW_RUNTIME_DIR}")
    endif()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS marketcapture EXPORT MarketCaptureTargets)
if(TARGET marketcapture_live)
    install(TARGETS marketcapture_live RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES config/feed.conf.example DESTINATION ${CMAKE_INSTALL_DATADIR}/marketcapture)
install(EXPORT MarketCaptureTargets NAMESPACE MarketCapture:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MarketCapture)
configure_package_config_file(
    cmake/MarketCaptureConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/MarketCaptureConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MarketCapture"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/MarketCaptureConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/MarketCaptureConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/MarketCaptureConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MarketCapture"
)
