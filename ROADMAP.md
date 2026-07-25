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

- DPDK kernel bypass and hardware timestamping
- FPGA feed-handler integration
