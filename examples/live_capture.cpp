#include "marketcapture/config.hpp"
#include "marketcapture/live_feed.hpp"
#include "marketcapture/pipeline.hpp"
#include "marketcapture/pcap.hpp"
#include "marketcapture/recorder.hpp"
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
        std::unique_ptr<marketcapture::TickRecorder> recorder;
        if (!config.record_path.empty())
            recorder = std::make_unique<marketcapture::TickRecorder>(config.record_path);
        std::unique_ptr<marketcapture::PcapWriter> pcap;
        if (!config.pcap_path.empty())
            pcap = std::make_unique<marketcapture::PcapWriter>(config.pcap_path);

        std::atomic<std::uint64_t> packets{0};
        std::atomic<std::uint64_t> rejected_packets{0};
        marketcapture::CapturePipeline pipeline([&](std::uint64_t, const marketcapture::Event& event) {
            if (recorder && marketcapture::is_order_tick(event)) recorder->write(event);
        });
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
                    if (pcap) {
                        const auto now = std::chrono::system_clock::now().time_since_epoch();
                        pcap->write(datagram, static_cast<std::uint64_t>(
                            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()));
                    }
                    try {
                        (void)pipeline.process(datagram);
                    } catch (const std::exception& error) {
                        rejected_packets.fetch_add(1);
                        std::cerr << "Rejected packet " << packet_number << ": " << error.what() << '\n';
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
        if (recorder) recorder->flush();
        if (pcap) pcap->flush();

        const auto& metrics = pipeline.metrics();
        std::cout << "packets=" << packets.load()
                  << " messages=" << metrics.messages()
                  << " gaps=" << metrics.gaps()
                  << " parse_errors=" << metrics.errors()
                  << " rejected_packets=" << rejected_packets.load() << '\n';
        return rejected_packets.load() == 0 ? 0 : 3;
    } catch (const std::exception& error) {
        std::cerr << "Live capture failed: " << error.what() << '\n';
        return 1;
    }
}
