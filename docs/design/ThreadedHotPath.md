# Threaded hot path

The production socket callback now performs bounded work: timestamp/optional
raw PCAP capture, one copy into a fixed packet slot, and SPSC publication.
`ThreadedCaptureEngine` owns the remaining stages:

```text
RX submitter
  -> 1023 usable preallocated 64 KiB packet slots
  -> SPSC ingress
  -> one arbitration + ITCH parser worker
       -> SPSC book queue     -> sharded-book worker
       -> SPSC recorder queue -> tick-recorder worker
       -> SPSC metrics queue  -> metrics worker
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
- Parsed events are value-owned by each downstream SPSC queue.
- Only the book worker mutates `ShardedBookRouter`.
- Only the recorder worker touches `TickRecorder`.
- Final book counts are published atomically after drain.

## Remaining allocation boundary

Packet storage and receive queues are preallocated, but the existing
`FeedArbitrator` owns out-of-order message payloads and the normalized `Event`
uses `std::string`; order-book maps also allocate as symbols/orders first
appear. Eliminating those allocations requires a separate fixed-width event ABI
and bounded order arena. This implementation isolates disk I/O and book
mutation from RX without claiming a completely allocation-free parser.

Raw PCAP writing in `marketcapture_live` remains optional and synchronous so
packet evidence is captured before the slot is reused. For strict line-rate
operation, disable `pcap_path` or add a dedicated raw-capture pool sized for the
storage device.
