# Changelog

## Unreleased

### Added

- mmap/Zstd block-boundary segment rotation with count and byte retention
- Linux benchmark/perf evidence bundle and flamegraph rendering workflow
- Licensed-feed acceptance evidence script and portfolio demo/resume narrative
- Release-triggered evidence artifact workflow

## v1.1.0

### Added

- Multi-symbol sharded order-book router
- A/B feed arbitrator with duplicate suppression and bounded gap recovery
- DLT_USER0 PCAP capture and deterministic replay
- Two-slot checksummed checkpoint recovery
- Seeded exchange simulator and end-to-end hiring showcase
- Exact p50/p99/p99.9 reporting and sharded-book throughput benchmark
- ASan/UBSan jobs, malformed-prefix tests, and libFuzzer targets
- Architecture, recovery, profiling, and reference-performance reports
- mmap-backed Zstd block storage with durable commit and replay validation
- Linux NIC hardware timestamp receive adapter
- Optional DPDK burst source with Ethernet/IPv4/UDP extraction
- FPGA DMA record ABI, lock-free consumer, and simulator coverage
- Linux recvmmsg batching and optional io_uring receive queues
- Constant-time order-reference-to-book routing on the update hot path

## v1.0.0

### Added

- Typed parsing and validation for all 22 ITCH 5.0 message types
- MoldUDP64 decoding and sequence-gap accounting
- SPSC event queue, price-level order book, and snapshot manager
- Attributed-add, priced-execution, and replace order-book handling
- Versioned order-tick recorder and deterministic replay engine
- Metrics, strategy dispatch, and cross-platform multicast receiver
- Integrated datagram pipeline with feed sequence-gap accounting
- Release-mode ITCH parser and SPSC microbenchmark
- Configurable live-capture executable, graceful shutdown, recording, and acceptance metrics
- End-to-end MoldUDP64-to-event load benchmark and exchange deployment guide
- CMake packaging, runnable example, tests, and CI

## v0.1.0

- Initial repository structure
