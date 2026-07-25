# Segment rotation and retention

`RotatingMappedStore` wraps `MappedZstdStore`. It rotates between complete
blocks, uses monotonic segment IDs, and removes only closed oldest segments.
Count and provisioned-byte ceilings are both enforced.

Each mmap file occupies its configured capacity, so retention uses file size.
A block larger than the raw segment limit is rejected. Production deployments
should archive closed segments and alert on disk pressure before deletion.
