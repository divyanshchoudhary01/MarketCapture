include(FetchContent)

FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.4.zip
)

set(BENCHMARK_ENABLE_TESTING OFF)

FetchContent_MakeAvailable(googlebenchmark)

add_executable(MarketCaptureBenchmarks
    benchmark_main.cpp
)

target_include_directories(MarketCaptureBenchmarks
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(MarketCaptureBenchmarks
    PRIVATE
        benchmark::benchmark
)
