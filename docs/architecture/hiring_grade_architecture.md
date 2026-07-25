# Hiring-grade architecture

## Objective

MarketCapture is an exchange-feed platform, not only a protocol parser. It must
continue producing deterministic state under packet duplication, A/B channel
skew, loss, process restart, malformed input, and peak load.

## Data flow

```text
Exchange simulator / licensed A+B multicast
                 |
       UDP or PCAP datagrams
                 |
       MoldUDP64 validation
                 |
    A/B sequence arbitrator --------> recovery requests
                 |
          ITCH normalization
                 |
       shard(symbol) dispatcher
          /       |       \
      shard 0  shard 1 ... shard N
          \       |       /
        checkpoint + recorder
                 |
      replay / strategy callbacks
```

## Thread model

- One receiver thread per multicast channel performs no book mutation.
- One arbitration thread owns sequence ordering and gap state.
- Each book shard has one writer. This removes locks from order mutation.
- Recorder and metrics are downstream consumers with bounded queues.
- Checkpoint creation occurs at an explicit sequence barrier.

The reference implementation is deterministic in a single process. Deployment
code can assign shard owners to pinned threads without changing book semantics.

## Memory model

- Datagram parsing uses `std::span` and does not retain packet views.
- Arbitration copies only messages that must wait for a missing sequence.
- Each symbol belongs to exactly one shard for its lifetime.
- Order references map directly to their owning shard.
- SPSC publication uses release/acquire ordering and cache-line-separated
  producer/consumer cursors.

## Recovery invariants

1. A sequence is emitted at most once.
2. No sequence after a gap is emitted until the missing range arrives.
3. Either A or B may satisfy a missing sequence.
4. Recovery requests are coalesced and bounded.
5. A checkpoint is valid only when its checksum, record count, and generation
   header are valid.
6. At least one previous checkpoint slot remains recoverable during replacement.

## Performance evidence

Release benchmarks report parser, queue, Mold-to-event, book-update, recorder,
and replay rates plus p50/p99/p99.9 latency. Results are evidence only when the
compiler, CPU, power mode, input seed, and iteration count are recorded.

## Out of scope

DPDK, FPGA, vendor retransmission protocols, and licensed multicast routes need
deployment-specific adapters. Their extension seams are documented but they are
not simulated as completed hardware integrations.
