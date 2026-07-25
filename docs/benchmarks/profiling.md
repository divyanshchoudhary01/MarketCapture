# Profiling and optimization notes

## Linux procedure

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
perf stat -d ./build/marketcapture_benchmark 10000000
perf record -F 999 -g --call-graph dwarf ./build/marketcapture_benchmark 10000000
perf report
```

Capture cycles, instructions, IPC, branch misses, cache misses, context
switches, migrations, and the annotated hottest call paths. Keep the raw
`perf.data` as a CI artifact or release attachment rather than committing a
machine-specific binary profile.

## Design decisions backed by the hot path

- ITCH and MoldUDP parsing use non-owning spans and explicit bounds checks.
- Mold messages are copied only when A/B reordering requires retained lifetime.
- SPSC storage is fixed at compile time; power-of-two masking replaces division.
- Producer and consumer cursors occupy separate cache lines.
- Symbols are assigned to stable shards, allowing one writer per shard.
- Informational ITCH messages bypass order-book mutation.
- Latency sample sorting happens after measurement, not in the update path.

## Known optimization opportunities

- The reference router scans books inside one shard for order-reference lookup.
  A per-shard order-to-book pointer index can remove this scan after pointer
  stability is guaranteed.
- `std::map` price levels prioritize correctness; flat or intrusive levels may
  improve cache locality for known price ranges.
- The portable recorder uses streams. A production Linux adapter can add
  `mmap`, batched `msync`, and independently checksummed compressed chunks.
- The receiver currently uses portable `recv`; Linux `recvmmsg`, busy polling,
  NIC RSS, CPU isolation, and hardware timestamps belong in a deployment
  adapter with before/after measurements.

Optimization work is accepted only with a benchmark demonstrating the claimed
improvement and tests showing unchanged deterministic state.
