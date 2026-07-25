#!/usr/bin/env bash
set -euo pipefail
build="${BUILD_DIR:-build-evidence}"
out="${1:-evidence/soak}"
iterations="${ITERATIONS:-100}"
mkdir -p "$out"
for run in $(seq 1 "$iterations"); do
  timeout --signal=KILL 0.$((run % 9 + 1))s \
    "$build/marketcapture_threaded" 10000000 "$out/crash-$run.ticks" || true
  "$build/marketcapture_showcase" 10000 "$out/recovery-$run" >> "$out/results.txt"
  rm -f "$out/crash-$run.ticks"
done
echo "Completed $iterations forced-termination/recovery cycles"
