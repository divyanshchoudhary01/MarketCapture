# A/B feed arbitration and gap recovery

## Problem

Exchange feeds are commonly delivered on redundant A and B channels. Packets
can arrive with different latency, be duplicated, or be missing on one channel.

## State machine

The arbitrator owns `next_sequence` and a bounded ordered buffer:

1. Decode and copy each MoldUDP64 message by sequence.
2. Drop sequences lower than `next_sequence` as duplicates.
3. Keep the first copy of future sequences, regardless of channel.
4. Emit consecutive messages beginning at `next_sequence`.
5. If the smallest buffered sequence is higher, request the missing inclusive
   range once and block later emission.

Recovery messages use the same ingest path, so they cannot bypass ordering or
duplicate checks.

## Failure policy

An overflowing reorder buffer is fatal because continuing would silently lose
determinism. Production retransmission transport is vendor-specific and is
connected through the recovery callback.
