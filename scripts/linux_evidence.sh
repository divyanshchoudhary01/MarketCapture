#!/usr/bin/env bash
set -euo pipefail
out="${1:-evidence/linux-$(date -u +%Y%m%dT%H%M%SZ)}"
mkdir -p "$out"
{ echo "utc=$(date -u --iso-8601=seconds)"; uname -a; lscpu; cmake --version; c++ --version; } > "$out/environment.txt" 2>&1
cmake -S . -B build-evidence -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native"
cmake --build build-evidence
ctest --test-dir build-evidence --output-on-failure | tee "$out/tests.txt"
./build-evidence/marketcapture_benchmark 10000000 | tee "$out/benchmark.txt"
./build-evidence/marketcapture_showcase 100000 "$out/showcase" | tee "$out/showcase.txt"
if command -v perf >/dev/null 2>&1; then
  perf stat -d -r 5 -o "$out/perf-stat.txt" ./build-evidence/marketcapture_benchmark 10000000
  perf record -F 999 -g --call-graph dwarf -o "$out/perf.data" ./build-evidence/marketcapture_benchmark 10000000
  perf report --stdio -i "$out/perf.data" > "$out/perf-report.txt"
else
  echo "perf unavailable; profiling artifacts were not produced" > "$out/perf-unavailable.txt"
fi
echo "Evidence written to $out"
