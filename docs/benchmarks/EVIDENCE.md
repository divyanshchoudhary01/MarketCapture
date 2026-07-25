# Reproducible performance evidence

The checked-in reference numbers are development baselines, not line-rate
hardware claims. Produce interview-grade evidence on a dedicated Linux host:

```sh
sudo apt-get install ninja-build linux-tools-common linux-tools-generic
taskset -c 2 ./scripts/linux_evidence.sh evidence/my-host
./scripts/render_flamegraph.sh evidence/my-host/perf.data /opt/FlameGraph \
  evidence/my-host/flamegraph.svg
```

Record CPU, kernel, compiler, governor, pinning, commands, sample count,
throughput, tail latency, cache/branch misses, and hot symbols. Explain each
optimization with same-host before/after numbers.

For a provider-entitled capture, set a finite `max_packets`, then run:

```sh
./scripts/licensed_feed_acceptance.sh /secure/path/feed.conf evidence/feed-run
```

Sanitize private addresses, session identifiers, and payloads before publishing.
