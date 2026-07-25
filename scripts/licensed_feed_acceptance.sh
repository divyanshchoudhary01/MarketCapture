#!/usr/bin/env bash
set -euo pipefail
config="${1:?usage: licensed_feed_acceptance.sh FEED_CONFIG [OUTPUT_DIR]}"
out="${2:-evidence/feed-$(date -u +%Y%m%dT%H%M%SZ)}"
mkdir -p "$out"
{ echo "utc=$(date -u --iso-8601=seconds)"; uname -a; ip -details link; ip route; ip maddr; } > "$out/host-before.txt" 2>&1
./build-evidence/marketcapture_live "$config" 2>&1 | tee "$out/capture.txt"
{ ip -s link; ss -u -a -n -i; } > "$out/host-after.txt" 2>&1
echo "Acceptance evidence written to $out"
