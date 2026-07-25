#!/usr/bin/env bash
set -euo pipefail
url="${1:?usage: fetch_official_itch.sh OFFICIAL_NASDAQ_GZ_URL EXPECTED_MD5 OUTPUT_GZ}"
expected="${2:?expected MD5 is required}"
output="${3:?output path is required}"
case "$url" in https://emi.nasdaq.com/ITCH/*) ;; *) echo "URL must be official emi.nasdaq.com ITCH" >&2; exit 2;; esac
mkdir -p "$(dirname "$output")"
curl --fail --location --continue-at - --output "$output" "$url"
actual="$(md5sum "$output" | awk '{print $1}')"
test "$actual" = "$expected" || { echo "checksum mismatch: $actual" >&2; exit 1; }
echo "Downloaded and verified $output"
echo "Validate without extracting: gzip -dc '$output' | ./build/marketcapture_validate_itch -"
