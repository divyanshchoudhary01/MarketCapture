# Reference performance report - 2026-07-25

## Environment

- Build: CMake Release (`-O3 -DNDEBUG`)
- Compiler: GCC/MinGW 15.2.0
- Host OS: Windows (development validation)
- Input: seeded synthetic valid ITCH order flow
- Samples: 50,000

## Results

| Component | Result |
|---|---:|
| ITCH parse | 72.55 ns/message |
| SPSC push+pop round trip | 3.22 ns |
| MoldUDP64 to normalized event | 318.50 ns/message |
| MoldUDP64 to event throughput | 3.14 million messages/s |
| Sharded book updates | 585,471 updates/s |
| End-to-end p50 | 1,400 ns |
| End-to-end p99 | 3,000 ns |
| End-to-end p99.9 | 24,800 ns |
| mmap Zstd durable write | 236 MiB/s |
| mmap Zstd replay | 300 MiB/s |

These numbers include wall-clock measurement overhead and unordered-map book
routing. They are a reproducible baseline, not a universal latency claim.
Results must be regenerated on a pinned Linux deployment host before comparing
against a contracted exchange peak rate.

## Reproduce

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/marketcapture_benchmark 50000
```

## Regression policy

Record compiler, CPU, governor/power plan, iteration count, median, and tail
latency. Investigate a repeatable regression greater than 10% using the
profiling procedure before merging a hot-path change.
