# mmap Zstd tick store

## Layout

The file is preallocated and memory mapped. A 4096-byte control page is followed
by independently compressed blocks:

```text
control page
  magic | version | capacity | committed offset | block count
block
  magic | raw bytes | compressed bytes | FNV-1a checksum | Zstd payload
```

## Commit protocol

1. Compress into process memory with the real Zstandard runtime.
2. Copy the block header and payload into unused mapped space.
3. Flush the complete block range.
4. Publish and flush the committed offset and block count.

On open, the reader scans complete blocks up to the committed offset and
validates decompression and checksum. A partial tail is never replayed.

## Capacity

The reference store uses a configured fixed capacity so append never needs to
move an active mapping. Production rotation creates a new segment and records a
manifest entry.
