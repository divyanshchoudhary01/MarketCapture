include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_executable(MarketCaptureTests
    test_main.cpp
)

target_link_libraries(MarketCaptureTests
    PRIVATE
        GTest::gtest_main
)

target_include_directories(MarketCaptureTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

include(GoogleTest)

gtest_discover_tests(MarketCaptureTests)
