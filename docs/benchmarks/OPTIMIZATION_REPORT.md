# Optimization report template

Run `bare_metal_benchmark.sh` before and after exactly one change on the same
isolated host. Include both evidence directories and flamegraphs.

| Metric | Before | After | Delta |
|---|---:|---:|---:|
| Messages/s | pending | pending | pending |
| p50 ns | pending | pending | pending |
| p99 ns | pending | pending | pending |
| p99.9 ns | pending | pending | pending |
| cycles/message | pending | pending | pending |
| cache misses/message | pending | pending | pending |
| branch misses/message | pending | pending | pending |

Explain the top flamegraph stack, the hypothesized bottleneck, the isolated
change, and whether tail latency regressed. Never mix VM and bare-metal runs.
