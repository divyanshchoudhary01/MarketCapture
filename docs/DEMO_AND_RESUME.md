# Portfolio presentation

## Three-minute demo

1. Explain the A/B loss and duplication failure model.
2. Run `marketcapture_showcase` and show deterministic recovery.
3. Show books, checkpoint restart equivalence, and compressed segments.
4. Show the Linux report and flamegraph with one measured optimization.
5. State honestly that physical NIC, licensed-feed, and FPGA results require
   their target deployment.

## Resume bullets

- Built a C++20 NASDAQ ITCH 5.0/MoldUDP64 platform with A/B arbitration,
  deterministic recovery, sharded books, crash-safe checkpoints, and PCAP replay.
- Engineered `recvmmsg`, optional fixed-buffer `io_uring`, timestamping, and DPDK
  receive paths; protected protocol boundaries with sanitizers and fuzzing.
- Implemented checksummed mmap/Zstd segments with bounded rotation/retention and
  measured throughput plus p50/p99/p99.9 latency reproducibly on Linux.

Add exact numbers only after running the evidence workflow on a documented host.
