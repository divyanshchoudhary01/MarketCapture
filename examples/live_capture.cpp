#include "marketcapture/config.hpp"
#include "marketcapture/live_feed.hpp"
#include "marketcapture/threaded_engine.hpp"
#include "marketcapture/types.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <thread>

namespace {
volatile std::sig_atomic_t stop_requested = 0;
void request_stop(int) { stop_requested = 1; }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: marketcapture_live <feed.conf>\n";
        return 2;
    }

    try {
        const auto config = marketcapture::LiveFeedConfig::load(argv[1]);
        std::atomic<std::uint64_t> packets{0};
        std::atomic<std::uint64_t> rejected_packets{0};
        marketcapture::ThreadedEngineConfig engine_config;
        engine_config.record_path = config.record_path;
        engine_config.pcap_path = config.pcap_path;
        marketcapture::ThreadedCaptureEngine engine(engine_config);
        marketcapture::UdpLiveFeed feed(config.multicast_group, config.port,
            config.interface_address, config.receive_buffer_bytes);

        std::cout << "Joining " << config.multicast_group << ':' << config.port
                  << " via " << config.interface_address << " (Ctrl+C to stop)\n";
        std::signal(SIGINT, request_stop);
        std::atomic<bool> finished{false};
        std::exception_ptr receiver_error;
        std::jthread receiver([&] {
            try {
                feed.run([&](std::span<const std::uint8_t> datagram) {
                    const auto packet_number = packets.fetch_add(1) + 1;
                    if (!engine.submit(marketcapture::FeedChannel::a, datagram)) {
                        rejected_packets.fetch_add(1);
                        engine.rethrow_worker_error();
                    }
                    if (config.max_packets != 0 && packet_number >= config.max_packets) feed.stop();
                });
            } catch (...) {
                receiver_error = std::current_exception();
            }
            finished.store(true);
        });
        while (!finished.load() && !stop_requested)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        feed.stop();
        receiver.join();
        if (receiver_error) std::rethrow_exception(receiver_error);
        if (!engine.wait_until_idle(std::chrono::seconds(30)))
            throw std::runtime_error("threaded capture pipeline did not drain");
        engine.stop();
        const auto metrics = engine.stats();
        std::cout << "packets=" << packets.load()
                  << " messages=" << metrics.messages_parsed
                  << " recovery_requests=" << metrics.recovery_requests
                  << " parse_errors=" << metrics.parse_errors
                  << " active_symbols=" << metrics.active_symbols
                  << " active_orders=" << metrics.active_orders
                  << " pcap_packets=" << metrics.pcap_packets
                  << " rejected_packets=" << rejected_packets.load() << '\n';
        return rejected_packets.load() == 0 ? 0 : 3;
    } catch (const std::exception& error) {
        std::cerr << "Live capture failed: " << error.what() << '\n';
        return 1;
    }
}
