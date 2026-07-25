# Threaded hot path

The production socket callback now performs bounded work: timestamp/optional
raw PCAP capture, one copy into a fixed packet slot, and SPSC publication.
`ThreadedCaptureEngine` owns the remaining stages:

```text
RX submitter
  -> 1023 usable preallocated 64 KiB packet slots
  -> SPSC ingress
  -> fixed-slot arbitration + fixed-width ITCH parser worker
       -> SPSC book queue     -> arena-book worker
       -> SPSC recorder queue -> tick-recorder worker
       -> SPSC metrics queue  -> metrics worker
       -> SPSC PCAP queue     -> raw-capture worker
```

The ingress API is non-blocking. Pool exhaustion rejects the packet and
increments `packets_rejected`; the caller chooses whether to drop, retry, or
fail. Downstream queues apply bounded backpressure by yielding the parser
worker, never the NIC submitter. Shutdown first closes ingress, drains parser
work, drains all three consumers, flushes/closes the recorder, and then exposes
final statistics.

## Ownership

- Exactly one thread calls `submit`.
- Packet bytes stay in their slot until arbitration/parsing completes.
- Parsed events use `HotEvent`: a fixed-width, string-free value.
- Only the book worker mutates `ArenaBookRouter`.
- Only the recorder worker touches `TickRecorder`.
- Only the PCAP worker touches `PcapWriter`.
- Final book counts are published atomically after drain.

## Bounded allocation contract

`FeedArbitrator` allocates its circular reorder window once and stores each ITCH
payload in a fixed 64-byte slot. `HotEvent` contains fixed symbol/attribution
arrays. `ArenaBookRouter` reserves its hash tables from a fixed PMR arena whose
upstream is `null_memory_resource`; its pool recycles erased nodes. No component
silently falls back to the general heap after construction. Capacity exhaustion
is an explicit exception/error counter, so operators size from observed peak
orders, levels, symbols, and reorder distance.

The recorder converts `HotEvent` into the legacy string-bearing `Event` only on
its cold worker. Raw PCAP bytes are copied into a preallocated SPSC slot and
written asynchronously. This keeps filesystem calls, compression, formatting,
and book mutation off the NIC submitter.

Administrative ITCH messages are fully length/type validated but represented as
`informational` in the hot ABI. Applications needing their complete strings can
run the full `ItchParser` on a separate control-plane queue.
