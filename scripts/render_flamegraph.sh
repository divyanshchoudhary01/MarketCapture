#!/usr/bin/env bash
set -euo pipefail
perf_data="${1:?usage: render_flamegraph.sh PERF_DATA FLAMEGRAPH_DIR [OUTPUT]}"
flamegraph_dir="${2:?path to FlameGraph checkout required}"
output="${3:-flamegraph.svg}"
perf script -i "$perf_data" | "$flamegraph_dir/stackcollapse-perf.pl" | "$flamegraph_dir/flamegraph.pl" --title "MarketCapture CPU profile" > "$output"
echo "Wrote $output"
