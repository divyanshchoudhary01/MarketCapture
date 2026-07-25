#!/usr/bin/env bash
set -euo pipefail
cpu="${CPU:-2}"
node="${NUMA_NODE:-0}"
out="${1:-evidence/bare-metal-$(date -u +%Y%m%dT%H%M%SZ)}"
mkdir -p "$out"
grep . /sys/devices/system/cpu/cpu"$cpu"/cpufreq/scaling_governor > "$out/governor.txt" 2>/dev/null || true
grep -E 'HugePages|Hugepagesize' /proc/meminfo > "$out/hugepages.txt"
numactl --hardware > "$out/numa.txt"
taskset -pc "$cpu" $$ > "$out/affinity.txt"
numactl --cpunodebind="$node" --membind="$node" \
  taskset -c "$cpu" ./scripts/linux_evidence.sh "$out"
