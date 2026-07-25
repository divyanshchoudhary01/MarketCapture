# MarketCapture

A C++20 low-latency market-data framework implementing every milestone from
the original v0.1–v1.0 roadmap.

## Roadmap status

- [x] v0.1 — Repository structure
- [x] v0.2 — Complete NASDAQ ITCH 5.0 message parser
- [x] v0.3 — MoldUDP64 decoder
- [x] v0.4 — Lock-free SPSC ring buffer
- [x] v0.5 — Snapshot manager
- [x] v0.6 — Order-book reconstruction
- [x] v0.7 — Binary order-tick recorder
- [x] v0.8 — Deterministic replay engine
- [x] v0.9 — Metrics and performance benchmark
- [x] v1.0 — UDP multicast live market-data pipeline

## Features

- Strict parsing for all 22 NASDAQ TotalView-ITCH 5.0 message types
- Bounds-checked MoldUDP64 packet decoding with zero-copy message views
- Wait-free bounded SPSC event queue
- Price-level order-book reconstruction and depth snapshots
- Versioned binary tick recording and deterministic replay
- Atomic counters and latency timing
- End-to-end packet pipeline with sequence-gap detection
- Strategy callback API
- Cross-platform UDP multicast receiver
- Configurable live-capture executable with recording, gap/error metrics, and Ctrl+C shutdown
- CMake install target, example, tests, and Linux/Windows CI

The parser exposes normalized typed events for system, stock, trading-status,
participant, control, order, trade, auction, and retail-interest messages.
Unknown types and malformed lengths are rejected.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Run the small order-book example:

```sh
./build/marketcapture_demo
```

Run the release-mode microbenchmark (optional argument: iteration count):

```sh
./build/marketcapture_benchmark 1000000
```

Run a live licensed feed:

```sh
cp config/feed.conf.example feed.conf
# Replace the multicast, port, and interface values with provider-assigned values.
./build/marketcapture_live feed.conf
```

See [Exchange connectivity](docs/EXCHANGE_CONNECTIVITY.md) for deployment and
acceptance criteria.

On multi-config Windows generators the executable is under `build/Release`.

## Basic use

```cpp
#include <marketcapture/itch.hpp>
#include <marketcapture/order_book.hpp>

marketcapture::ItchParser parser;
marketcapture::OrderBook book;

// message is one complete ITCH message obtained from a MoldUDP64 packet.
auto event = parser.parse(message);
book.apply(event);
```

`CapturePipeline` combines MoldUDP64 decoding, ITCH parsing, sequence-gap
tracking, metrics, and event delivery. A live application passes each UDP
datagram from `UdpLiveFeed` to the pipeline, then publishes normalized events
through `SpscRingBuffer` and updates one `OrderBook` per symbol. `TickRecorder`
can persist the same events and `ReplayEngine` delivers them back in recorded
order.

## Design constraints

- `MoldPacket::messages` are non-owning spans and remain valid only while the
  source datagram buffer remains alive.
- `SpscRingBuffer` supports exactly one producer and one consumer.
- `OrderBook` represents one symbol and rejects mixed-symbol or invalid order
  transitions. Informational ITCH events have no book effect.
- ITCH prices use the native four-decimal fixed-point representation.
- The recorder stores normalized order-impacting events; informational ITCH
  events are not order ticks and are intentionally rejected.
- The recorder format is versioned by its `MCAPTIK1` header. Files are intended
  for trusted local capture/replay workloads.
- Benchmark values are environment-specific and should be compared on pinned,
  equivalently configured hardware.
- Market-data entitlement and multicast network delivery are provisioned by the
  exchange or feed provider; they are not application passwords embedded in
  source code.

## License

MIT
