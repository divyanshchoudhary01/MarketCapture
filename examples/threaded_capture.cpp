#include "marketcapture/exchange_simulator.hpp"
#include "marketcapture/threaded_engine.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    try {
        const auto count = argc > 1 ? std::stoull(argv[1]) : 100'000;
        const auto output = argc > 2 ? std::filesystem::path(argv[2])
                                     : std::filesystem::path("threaded-output.ticks");
        marketcapture::ThreadedEngineConfig config;
        config.record_path = output;
        config.initial_sequence = 1;
        marketcapture::ThreadedCaptureEngine engine(config);
        marketcapture::ExchangeSimulator simulator(0xC0FFEE);

        const auto start = std::chrono::steady_clock::now();
        for (std::size_t index = 0; index < count; ++index) {
            auto packet = simulator.next();
            while (!engine.submit(marketcapture::FeedChannel::a, packet.datagram)) {
                engine.rethrow_worker_error();
                std::this_thread::yield();
            }
        }
        if (!engine.wait_until_idle(std::chrono::seconds(30)))
            throw std::runtime_error("threaded pipeline did not drain");
        engine.stop();
        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        const auto stats = engine.stats();
        std::cout << "packets=" << stats.packets_submitted
                  << " parsed=" << stats.messages_parsed
                  << " book_updates=" << stats.book_updates
                  << " recorded=" << stats.recorded_events
                  << " symbols=" << stats.active_symbols
                  << " orders=" << stats.active_orders
                  << " ingress_hwm=" << stats.ingress_high_watermark
                  << " book_hwm=" << stats.book_high_watermark
                  << " recorder_hwm=" << stats.recorder_high_watermark
                  << " packets_per_second="
                  << static_cast<std::uint64_t>(stats.packets_submitted / elapsed)
                  << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Threaded capture failed: " << error.what() << '\n';
        return 1;
    }
}
