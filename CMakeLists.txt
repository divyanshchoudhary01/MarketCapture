cmake_minimum_required(VERSION 3.20)
project(MarketCapture VERSION 1.1.0 LANGUAGES CXX)

option(MARKETCAPTURE_BUILD_TESTS "Build unit tests" ON)
option(MARKETCAPTURE_BUILD_EXAMPLES "Build examples" ON)
option(MARKETCAPTURE_BUILD_BENCHMARKS "Build performance benchmark" ON)
option(MARKETCAPTURE_BUILD_FUZZERS "Build libFuzzer targets (Clang only)" OFF)
option(MARKETCAPTURE_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(MARKETCAPTURE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(MARKETCAPTURE_ENABLE_DPDK "Build the DPDK burst packet source" OFF)

add_library(marketcapture
    src/book_router.cpp
    src/checkpoint.cpp
    src/itch.cpp
    src/config.cpp
    src/exchange_simulator.cpp
    src/feed_arbitrator.cpp
    src/fpga_ring.cpp
    src/hardware_timestamp.cpp
    src/dpdk_source.cpp
    src/moldudp64.cpp
    src/pcap.cpp
    src/pipeline.cpp
    src/order_book.cpp
    src/recorder.cpp
    src/live_feed.cpp
    src/mmap_store.cpp
)
add_library(MarketCapture::marketcapture ALIAS marketcapture)
target_include_directories(marketcapture PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(marketcapture PUBLIC cxx_std_20)
target_link_libraries(marketcapture PRIVATE ${CMAKE_DL_LIBS})
if(MARKETCAPTURE_ENABLE_DPDK)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DPDK REQUIRED IMPORTED_TARGET libdpdk)
    target_link_libraries(marketcapture PRIVATE PkgConfig::DPDK)
    target_compile_definitions(marketcapture PUBLIC MARKETCAPTURE_HAS_DPDK=1)
endif()
if(MSVC)
    target_compile_options(marketcapture PRIVATE /W4 /permissive-)
else()
    target_compile_options(marketcapture PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(WIN32)
    target_link_libraries(marketcapture PUBLIC ws2_32)
endif()
if(MARKETCAPTURE_ENABLE_ASAN AND NOT MSVC)
    target_compile_options(marketcapture PUBLIC -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(marketcapture PUBLIC -fsanitize=address)
endif()
if(MARKETCAPTURE_ENABLE_UBSAN AND NOT MSVC)
    target_compile_options(marketcapture PUBLIC -fsanitize=undefined -fno-omit-frame-pointer)
    target_link_options(marketcapture PUBLIC -fsanitize=undefined)
endif()

if(MARKETCAPTURE_BUILD_EXAMPLES)
    add_executable(marketcapture_demo examples/demo.cpp)
    target_link_libraries(marketcapture_demo PRIVATE MarketCapture::marketcapture)

    add_executable(marketcapture_live examples/live_capture.cpp)
    find_package(Threads REQUIRED)
    target_link_libraries(marketcapture_live PRIVATE MarketCapture::marketcapture Threads::Threads)

    add_executable(marketcapture_showcase examples/hiring_demo.cpp)
    target_link_libraries(marketcapture_showcase PRIVATE MarketCapture::marketcapture)
endif()

if(MARKETCAPTURE_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Fuzz targets require Clang")
    endif()
    target_compile_options(marketcapture PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(marketcapture PUBLIC -fsanitize=address,undefined)
    foreach(FUZZ_TARGET itch moldudp64)
        add_executable(fuzz_${FUZZ_TARGET} fuzz/fuzz_${FUZZ_TARGET}.cpp)
        target_link_libraries(fuzz_${FUZZ_TARGET} PRIVATE MarketCapture::marketcapture)
        target_compile_options(fuzz_${FUZZ_TARGET} PRIVATE -fsanitize=fuzzer,address,undefined)
        target_link_options(fuzz_${FUZZ_TARGET} PRIVATE -fsanitize=fuzzer,address,undefined)
    endforeach()
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
if(TARGET marketcapture_showcase)
    install(TARGETS marketcapture_showcase RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
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
