# PCAP capture and replay

MarketCapture writes classic PCAP with link type `DLT_USER0` (147). Each record
contains one complete MoldUDP64 UDP payload, without synthetic Ethernet/IP
headers. This keeps replay independent of capture-host MAC and IP addresses.

The reader validates the global header, link type, record lengths, and
truncation before delivering datagrams in file order. Packet timestamps are
preserved as nanoseconds. Deterministic replay does not sleep; timing-aware
drivers may schedule from the recorded timestamps.

`PcapRecoverySource` indexes datagram sequence ranges and can satisfy an
arbitrator recovery request immediately. It is the deterministic test adapter
for the vendor-specific retransmission service used in production.

Standard packet tools can open the file, but require a user-defined dissector to
interpret `DLT_USER0` as MoldUDP64.
