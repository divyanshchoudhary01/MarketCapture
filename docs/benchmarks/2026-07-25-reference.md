# Reference performance report - 2026-07-25

## Environment

- Build: CMake Release (`-O3 -DNDEBUG`)
- Compiler: GCC/MinGW 15.2.0
- Host OS: Windows (development validation)
- Input: seeded synthetic valid ITCH order flow
- Samples: 100,000

## Results

| Component | Result |
|---|---:|
| ITCH parse | 31.93 ns/message |
| SPSC push+pop round trip | 3.14 ns |
| MoldUDP64 to normalized event | 158.24 ns/message |
| MoldUDP64 to event throughput | 6.32 million messages/s |
| Sharded book updates | 810,950 updates/s |
| End-to-end p50 | 900 ns |
| End-to-end p99 | 2,200 ns |
| End-to-end p99.9 | 18,000 ns |
| mmap Zstd durable write | 348 MiB/s |
| mmap Zstd replay | 479 MiB/s |

These numbers include wall-clock measurement overhead and unordered-map book
routing. They are a reproducible baseline, not a universal latency claim.
Results must be regenerated on a pinned Linux deployment host before comparing
against a contracted exchange peak rate.

## Reproduce

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/marketcapture_benchmark 100000
```

## Regression policy

Record compiler, CPU, governor/power plan, iteration count, median, and tail
latency. Investigate a repeatable regression greater than 10% using the
profiling procedure before merging a hot-path change.
