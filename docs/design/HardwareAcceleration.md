# Hardware acceleration and timestamp adapters

## Principle

The normalized event pipeline must not depend on one NIC, DPDK release, or FPGA
vendor. Hardware adapters terminate at stable packet/event boundaries.

## DPDK

`DpdkPacketSource` is built only with `MARKETCAPTURE_ENABLE_DPDK=ON`. It owns an
EAL port/queue and exposes bursts as packet spans to the same MoldUDP64 pipeline
used by the socket and PCAP sources. The default build has no DPDK dependency.

## NIC hardware timestamps

On Linux, `HardwareTimestampReceiver` requests `SO_TIMESTAMPING` and extracts
the raw hardware timestamp from `SCM_TIMESTAMPING`. It reports whether each
datagram carries hardware or software time. Enabling actual hardware timestamps
also requires NIC/driver configuration outside the process.

## Linux socket batching

`RecvmmsgReceiver` uses `recvmmsg(MSG_WAITFORONE)` to drain multiple datagrams
per syscall. `IoUringUdpReceiver`, enabled with
`MARKETCAPTURE_ENABLE_IO_URING`, maintains a queue of fixed receive buffers and
rearms completed SQEs in batches. The fixed-buffer implementation supports
older io_uring-capable kernels; provided-buffer multishot receive can be added
for kernel 6.0+ after target-host measurement.

## FPGA ABI

`FpgaEventRing` defines a fixed-size, versioned DMA record with sequence,
timestamp, message size, flags, and ITCH bytes. The consumer validates the ABI
before publishing records. The simulator writes the same ABI, allowing CI to
test wraparound, malformed descriptors, and sequence handling without hardware.

## Validation boundary

CI proves adapter compilation, ABI parsing, and simulated behavior. Line-rate,
NUMA, PCIe, driver, and timestamp-accuracy claims require the target host and
device and must be published as separate performance evidence.

## Primary API references

- [Linux recvmmsg manual](https://man7.org/linux/man-pages/man2/recvmmsg.2.html)
- [liburing receive manual](https://man7.org/linux/man-pages/man3/io_uring_prep_recv.3.html)
- [io_uring multishot overview](https://man7.org/linux/man-pages/man7/io_uring_multishot.7.html)
