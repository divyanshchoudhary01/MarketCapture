#include "marketcapture/dpdk_source.hpp"
#include "marketcapture/threaded_engine.hpp"
#include <atomic>
#include <csignal>
#include <iostream>
#ifdef MARKETCAPTURE_HAS_DPDK
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#endif

namespace { volatile std::sig_atomic_t stopping = 0; void stop(int) { stopping = 1; } }

int main(int argc, char** argv) {
#ifdef MARKETCAPTURE_HAS_DPDK
    try {
        const auto consumed = rte_eal_init(argc, argv);
        if (consumed < 0) throw std::runtime_error("DPDK EAL initialization failed");
        std::uint16_t port = 0;
        if (!rte_eth_dev_is_valid_port(port))
            throw std::runtime_error("DPDK port 0 is unavailable");
        auto* pool = rte_pktmbuf_pool_create("marketcapture_pool", 16384, 256, 0,
                                             RTE_MBUF_DEFAULT_BUF_SIZE,
                                             rte_socket_id());
        if (!pool) throw std::runtime_error("DPDK mbuf pool creation failed");
        rte_eth_conf configuration{};
        if (rte_eth_dev_configure(port, 1, 0, &configuration) < 0 ||
            rte_eth_rx_queue_setup(port, 0, 2048, rte_eth_dev_socket_id(port),
                                   nullptr, pool) < 0 ||
            rte_eth_dev_start(port) < 0)
            throw std::runtime_error("DPDK port configuration failed");
        rte_eth_promiscuous_enable(port);

        marketcapture::ThreadedEngineConfig engine_config;
        engine_config.overload_policy = marketcapture::OverloadPolicy::reject;
        marketcapture::ThreadedCaptureEngine engine(engine_config);
        marketcapture::DpdkPacketSource source(port, 0, 32);
        std::signal(SIGINT, stop);
        while (!stopping)
            source.poll([&](std::span<const std::uint8_t> payload) {
                (void)engine.submit(marketcapture::FeedChannel::a, payload);
            });
        engine.wait_until_idle(std::chrono::seconds(30));
        engine.stop();
        rte_eth_stats nic{};
        rte_eth_stats_get(port, &nic);
        const auto stats = engine.stats();
        std::cout << "rx_packets=" << nic.ipackets << " rx_missed=" << nic.imissed
                  << " rx_errors=" << nic.ierrors
                  << " submitted=" << stats.packets_submitted
                  << " rejected=" << stats.packets_rejected
                  << " ingress_hwm=" << stats.ingress_high_watermark << '\n';
        rte_eth_dev_stop(port);
        rte_eth_dev_close(port);
        rte_eal_cleanup();
    } catch (const std::exception& error) {
        std::cerr << "DPDK capture failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
#else
    (void)argc; (void)argv;
    std::cerr << "Rebuild with MARKETCAPTURE_ENABLE_DPDK=ON\n";
    return 2;
#endif
}
