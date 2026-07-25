#!/usr/bin/env bash
set -euo pipefail
out="${1:-evidence/demo.cast}"
command -v asciinema >/dev/null || { echo "install asciinema" >&2; exit 2; }
asciinema rec --overwrite --title "MarketCapture deterministic recovery" \
  --command "./build-evidence/marketcapture_showcase 10000 evidence/demo-output" "$out"
echo "Recorded $out; review it before publishing or converting to video."
