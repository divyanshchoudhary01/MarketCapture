# Sharded order books

## Problem

A single book cannot reconstruct a full equity feed. Updates must be routed to
thousands of symbols without global book locks or ambiguous order ownership.

## Design

`ShardedBookRouter` hashes a symbol to a stable shard and maintains a global
order-reference-to-shard index for messages that do not contain a symbol.
Each shard owns its symbol books. Add, execute, cancel, delete, and replace
events are applied in feed sequence order.

## Complexity

- Symbol route: average O(1)
- Order route: average O(1)
- Price-level update: O(log P), where P is active price levels
- Snapshot: O(depth)

## Thread model

One writer owns a shard. Readers consume immutable snapshots. The router itself
is intentionally not an MPMC container; external dispatch assigns events to
shard workers.

## Alternatives

A lock per book is simpler but introduces contention and unpredictable tail
latency. A single global order map remains in the reference router for
deterministic routing; production shard workers can partition this index by
order-reference hash.
