# Roadmap

Every original milestone is complete and covered by the build or test suite.

## v0.1 — Repository Structure

- [x] C++20 library, public headers, examples, tests, installable CMake package
- [x] Linux and Windows continuous integration

## v0.2 — NASDAQ ITCH Parser

- [x] All 22 TotalView-ITCH 5.0 message types
- [x] Big-endian integer decoding, fixed-width text normalization, strict sizes
- [x] Validation for side, boolean, and printable fields

## v0.3 — MoldUDP64 Decoder

- [x] Session and sequence decoding
- [x] Bounds-checked, zero-copy message spans

## v0.4 — Lock-Free Ring Buffer

- [x] Bounded, wait-free SPSC queue with acquire/release publication

## v0.5 — Snapshot Manager

- [x] Sequence-tagged bid/ask depth snapshots

## v0.6 — Order Book

- [x] Price-level aggregation
- [x] Add, attributed add, execute, priced execute, cancel, delete, and replace
- [x] Best bid/ask and configurable depth

## v0.7 — Recorder

- [x] Versioned binary recording of normalized order-impacting events
- [x] Corruption and truncation checks

## v0.8 — Replay Engine

- [x] Ordered callback replay with exact event counts
- [x] Round-trip tests for every recordable order-event family

## v0.9 — Metrics

- [x] Atomic packet, message, error, and sequence-gap counters
- [x] Nanosecond latency timer
- [x] Release-mode ITCH and SPSC microbenchmark

## v1.0 — Live Market Data

- [x] Cross-platform UDP multicast receiver
- [x] Interruptible shutdown
- [x] Validated file-based deployment configuration and receive-buffer tuning
- [x] Runnable live capture with tick recording and acceptance metrics
- [x] Integrated MoldUDP64 → ITCH event pipeline
- [x] Session sequence tracking and gap detection

## Future hardware-specific work

- [x] Optional DPDK burst RX adapter and UDP payload extraction
- [x] Linux `SO_TIMESTAMPING` hardware timestamp receiver
- [x] FPGA DMA record ABI, ring consumer, and simulator
- [x] mmap append-only Zstd block store
- [x] Linux `recvmmsg` batch receiver
- [x] Optional fixed-buffer io_uring receive queue
- [ ] Publish line-rate results from a supported DPDK NIC
- [ ] Publish timestamp accuracy from a PTP-synchronized NIC
- [ ] Validate the DMA ABI against a selected FPGA card and driver

## Hiring-grade reliability phase

- [x] Multi-symbol sharded order-book routing
- [x] A/B duplicate suppression, arbitration, and automatic gap requests
- [x] PCAP capture and deterministic offline playback
- [x] Checksummed two-generation checkpoints and restart recovery
- [x] Seeded exchange simulator with reproducible valid order flow
- [x] Malformed-input suites, ASan/UBSan CI, and libFuzzer targets
- [x] Throughput and p50/p99/p99.9 benchmark reporting
- [x] Profiling methodology and optimization analysis
- [x] End-to-end loss/recovery/restart showcase
