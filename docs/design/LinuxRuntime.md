# Linux and DPDK runtime

Prepare a dedicated test host only after reviewing the script:

```sh
sudo CPU=2 HUGEPAGES=1024 ./scripts/prepare_linux_host.sh
CPU=2 NUMA_NODE=0 ./scripts/bare_metal_benchmark.sh evidence/host-a
```

For stronger isolation add `isolcpus=2 nohz_full=2 rcu_nocbs=2` to the kernel
command line and move NIC IRQs away from CPU 2. These changes require a reboot
and are intentionally not automated.

Thread CPUs are configurable through `ThreadedEngineConfig`; `-1` leaves
scheduling to the OS. Queue high-watermarks and rejected ingress packets are
reported in `ThreadedEngineStats`. `reject` preserves RX progress; `spin`
applies producer backpressure and is suitable only when upstream loss behavior
is understood.

Build and run DPDK:

```sh
cmake -S . -B build-dpdk -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DMARKETCAPTURE_ENABLE_DPDK=ON
cmake --build build-dpdk
sudo ./build-dpdk/marketcapture_dpdk -l 2-7 -n 4 \
  --file-prefix marketcapture --
```

The harness configures port 0/queue 0, creates an mbuf pool, runs burst RX, and
prints NIC packets, missed packets, errors, engine rejects, and ingress
watermark at shutdown. Binding the NIC to `vfio-pci`, NUMA placement, RSS, and
queue count remain host-specific operational steps.
