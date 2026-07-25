# Checkpoint and restart recovery

## Problem

An interrupted checkpoint must not erase the last known-good book state.

## Format

The store alternates between `<base>.a` and `<base>.b`. Each file contains:

- magic and format version
- monotonically increasing generation
- feed sequence barrier
- record count
- FNV-1a payload checksum
- normalized active orders

## Commit protocol

1. Read both slots and choose the highest valid generation.
2. Serialize the next generation to a temporary file.
3. Flush and close the stream, then call `fsync` or `FlushFileBuffers`.
4. Atomically replace only the older target slot with write-through semantics.
5. Flush the parent directory on POSIX.
6. On restart, validate both and load the highest valid generation.

If power is lost during steps 2-4, the other slot remains valid.

## Future improvement

Production deployment may additionally store compressed shard blocks with
independent checksums and replicate checkpoints to separate storage.
