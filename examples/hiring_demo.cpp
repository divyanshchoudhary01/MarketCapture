#include "marketcapture/book_router.hpp"
#include "marketcapture/checkpoint.hpp"
#include "marketcapture/exchange_simulator.hpp"
#include "marketcapture/feed_arbitrator.hpp"
#include "marketcapture/itch.hpp"
#include "marketcapture/pcap.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    try {
        const std::size_t packet_count = argc > 1 ? std::stoull(argv[1]) : 10'000;
        const auto output = argc > 2 ? std::filesystem::path(argv[2])
                                     : std::filesystem::path("demo-output");
        std::filesystem::create_directories(output);
        const auto pcap_path = output / "simulated-feed.pcap";
        const auto checkpoint_path = output / "books.checkpoint";

        marketcapture::ExchangeSimulator simulator(0xC0FFEE,
            {"AAPL", "MSFT", "NVDA", "AMZN", "META", "TSLA"});
        const auto packets = simulator.generate(packet_count);
        {
            marketcapture::PcapWriter writer(pcap_path);
            std::uint64_t timestamp = 1'700'000'000'000'000'000ull;
            for (const auto& packet : packets) {
                writer.write(packet.datagram, timestamp);
                timestamp += 1'000;
            }
            writer.flush();
        }

        marketcapture::PcapRecoverySource recovery_source(pcap_path);
        marketcapture::ItchParser parser;
        marketcapture::ShardedBookRouter books(8);
        std::vector<marketcapture::RecoveryRequest> recovery_requests;
        std::uint64_t delivered = 0;
        std::unique_ptr<marketcapture::FeedArbitrator> arbitrator;
        arbitrator = std::make_unique<marketcapture::FeedArbitrator>(
            [&](const marketcapture::ArbitratedMessage& message) {
                books.apply(parser.parse(message.payload));
                ++delivered;
            },
            [&](const marketcapture::RecoveryRequest& request) {
                recovery_requests.push_back(request);
                const auto recovered = recovery_source.recover(
                    request.first_sequence, request.last_sequence,
                    [&](std::span<const std::uint8_t> datagram) {
                        (void)arbitrator->ingest(
                            marketcapture::FeedChannel::recovery, datagram);
                    });
                if (recovered == 0)
                    throw std::runtime_error("recovery source cannot satisfy gap");
            }, 1, packet_count + 1);

        const auto start = std::chrono::steady_clock::now();
        for (std::size_t index = 0; index < packets.size(); ++index) {
            if ((index + 1) % 17 != 0)
                (void)arbitrator->ingest(marketcapture::FeedChannel::a,
                                         packets[index].datagram);
            if ((index + 1) % 23 != 0)
                (void)arbitrator->ingest(marketcapture::FeedChannel::b,
                                         packets[index].datagram);
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;

        if (delivered != packet_count || arbitrator->buffered() != 0)
            throw std::runtime_error("recovery did not produce a complete ordered stream");
        marketcapture::CheckpointStore checkpoints(checkpoint_path);
        const auto saved = checkpoints.save(books, delivered);
        marketcapture::ShardedBookRouter restored(8);
        const auto loaded = checkpoints.load(restored);
        if (loaded.sequence != delivered ||
            restored.order_count() != books.order_count() ||
            restored.symbol_count() != books.symbol_count())
            throw std::runtime_error("checkpoint restart state mismatch");

        const auto seconds = std::chrono::duration<double>(elapsed).count();
        std::cout << "MarketCapture hiring-grade demonstration\n"
                  << "  generated_packets: " << packet_count << '\n'
                  << "  ordered_messages: " << delivered << '\n'
                  << "  duplicate_messages_dropped: " << arbitrator->duplicates() << '\n'
                  << "  recovery_requests: " << recovery_requests.size() << '\n'
                  << "  active_symbols: " << books.symbol_count() << '\n'
                  << "  active_orders: " << books.order_count() << '\n'
                  << "  checkpoint_generation: " << saved.generation << '\n'
                  << "  restart_verified: yes\n"
                  << "  processing_messages_per_second: "
                  << static_cast<std::uint64_t>(packet_count / seconds) << '\n'
                  << "  pcap: " << pcap_path.string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Demo failed: " << error.what() << '\n';
        return 1;
    }
}
