# Exchange connectivity

MarketCapture contains the software path needed to receive a licensed
MoldUDP64/ITCH multicast feed. The exchange or data vendor must separately
provision the entitlement and network route.

## Required provider information

Obtain these values for every channel:

- MoldUDP64 multicast IPv4 group
- UDP destination port
- Local interface IPv4 address on the market-data VLAN
- Expected session name and initial sequence policy
- Primary/secondary feed channel mapping
- Firewall, source-network, and entitlement identifiers required by the provider

The multicast feed itself does not authenticate with a username/password in the
packet decoder. Do not put portal or extranet credentials in `feed.conf`.

## Configure

Copy `config/feed.conf.example` outside source control and replace its example
values:

```ini
multicast_group=233.54.12.1
port=18000
interface_address=10.20.30.40
receive_buffer_bytes=16777216
record_path=capture.ticks
max_packets=1000000
```

`max_packets=0` captures until Ctrl+C. Use an absolute `record_path` in a
production service. Ensure the capture volume has sufficient sustained write
bandwidth and free space.

## Host preparation

1. Confirm the assigned interface is up and has the provider-assigned route.
2. Permit inbound UDP for the assigned group/port in the host firewall.
3. Ensure multicast/IGMP is enabled across the switch and routed network.
4. Increase OS socket and NIC receive queues if the requested receive buffer is
   capped by the host.
5. Pin the capture process and NIC interrupts to suitable CPU cores for latency
   testing; keep power management and test conditions consistent.

## Run an acceptance capture

Build Release and start the receiver:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/marketcapture_live feed.conf
```

For an initial finite run, set `max_packets`. A healthy summary has:

- `packets` and `messages` increasing
- `parse_errors=0`
- `rejected_packets=0`
- `gaps=0` after starting at the provider-designated sequence boundary

Nonzero gaps require recovery from the provider's retransmission/snapshot
service before the reconstructed book can be considered complete. Capture the
console summary, feed session, start time, interface counters, and benchmark
output as acceptance evidence.

## Load validation

Run the included in-process benchmark on the deployment host:

```sh
./build/marketcapture_benchmark 10000000
```

It reports ITCH parse latency, SPSC round-trip latency, and complete
MoldUDP64-to-event throughput. Compare the last figure to the contracted peak
message rate with operational headroom. This benchmark validates the software
hot path; the finite live acceptance run validates the NIC, multicast route,
kernel socket, real packet shapes, and recorder together.

## What cannot be prepackaged

Exchange entitlement, private multicast routes, provider retransmission
endpoints, production symbols, and real peak traffic are external services.
They cannot be generated or verified from a public repository. Once those
values are available, `marketcapture_live` is the executable entry point for
the real-feed acceptance run.
