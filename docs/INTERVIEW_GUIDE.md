# MarketCapture interview guide

## Three-minute narrative

1. Problem: reconstruct a deterministic order book from redundant UDP feeds.
2. Hot path: fixed packet pool, circular A/B reorder window, fixed ITCH ABI.
3. Concurrency: one producer per SPSC; independent arena-book, recorder,
   metrics, and PCAP workers with explicit overload policy.
4. Correctness: official BinaryFile streaming, independent golden decoder,
   fuzzing, sanitizers, checkpoints, replay, and forced-termination soak.
5. Evidence: pinned-core Linux runs, queue watermarks, tail latency,
   flamegraphs, DPDK/io_uring compile/runtime paths.

## Questions you must answer

- Why acquire/release rather than sequential consistency on SPSC cursors?
- What happens when the packet pool or reorder window fills?
- Why copy an out-of-order ITCH message into a fixed slot?
- How are duplicate A/B messages distinguished from gaps?
- Why does the PMR book use a recycling pool over a monotonic arena?
- What is deterministic across checkpoint/replay and what is not?
- When is `recvmmsg` preferable to multishot `io_uring` or DPDK?
- Which measurements require a physical NIC and PTP clock?

## Honest boundaries

The repository proves software behavior and compile-time hardware adapters.
It does not prove line rate, timestamp accuracy, licensed-feed compatibility,
or FPGA DMA performance until the corresponding external system is tested.
