#include "marketcapture/config.hpp"
#include "marketcapture/book_router.hpp"
#include "marketcapture/checkpoint.hpp"
#include "marketcapture/exchange_simulator.hpp"
#include "marketcapture/feed_arbitrator.hpp"
#include "marketcapture/itch.hpp"
#include "marketcapture/latency.hpp"
#include "marketcapture/metrics.hpp"
#include "marketcapture/moldudp64.hpp"
#include "marketcapture/order_book.hpp"
#include "marketcapture/pipeline.hpp"
#include "marketcapture/pcap.hpp"
#include "marketcapture/recorder.hpp"
#include "marketcapture/ring_buffer.hpp"
#include "marketcapture/snapshot.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
template <typename F> void throws(F f) {
    try { f(); ++failures; std::cerr << "FAIL: expected exception\n"; } catch (const std::exception&) {}
}
void put(std::vector<std::uint8_t>& v, std::size_t p, std::uint64_t x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) v[p+n-1-i] = static_cast<std::uint8_t>(x >> (i*8));
}

void test_itch() {
    std::vector<std::uint8_t> add(36);
    add[0]='A'; put(add, 5, 123456, 6); put(add, 11, 42, 8); add[19]='B';
    put(add, 20, 100, 4);
    const char symbol[] = "AAPL    ";
    for (int i=0; i<8; ++i) add[24+i]=symbol[i];
    put(add, 32, 1892500, 4);
    auto event = marketcapture::ItchParser{}.parse(add);
    const auto& order = std::get<marketcapture::AddOrder>(event);
    CHECK(order.timestamp == 123456);
    CHECK(order.order_id == 42);
    CHECK(order.symbol == "AAPL");
    CHECK(order.price == 1892500);
    throws([&] { (void)marketcapture::ItchParser{}.parse(std::span(add).first(4)); });

    const std::vector<std::pair<char, std::size_t>> catalogue{
        {'S',12}, {'R',39}, {'H',25}, {'Y',20}, {'L',26}, {'V',35}, {'W',12},
        {'K',28}, {'J',35}, {'h',21}, {'F',40}, {'E',31}, {'C',36}, {'X',23},
        {'D',19}, {'U',35}, {'P',44}, {'Q',40}, {'B',19}, {'I',50}, {'N',20}
    };
    for (const auto [type, size] : catalogue) {
        std::vector<std::uint8_t> message(size, ' ');
        message[0] = static_cast<std::uint8_t>(type);
        if (type == 'R') { message[25] = 'N'; message[38] = 'N'; }
        if (type == 'L') message[23] = 'Y';
        if (type == 'F' || type == 'P') message[19] = 'S';
        if (type == 'C') message[31] = 'Y';
        try { (void)marketcapture::ItchParser{}.parse(message); }
        catch (const std::exception& e) {
            std::cerr << "FAIL catalogue type " << type << ": " << e.what() << '\n';
            ++failures;
        }
    }
}

void test_mold() {
    std::vector<std::uint8_t> packet(25);
    const char session[] = "SESSION1234";
    for (int i=0; i<10; ++i) packet[i]=session[i];
    put(packet, 10, 99, 8); put(packet, 18, 1, 2); put(packet, 20, 3, 2);
    packet[22]='a'; packet[23]='b'; packet[24]='c';
    auto decoded = marketcapture::MoldUdp64Decoder{}.decode(packet);
    CHECK(decoded.sequence == 99);
    CHECK(decoded.messages.size() == 1);
    CHECK(decoded.messages[0][2] == 'c');
    packet.pop_back();
    throws([&] { (void)marketcapture::MoldUdp64Decoder{}.decode(packet); });
}

std::vector<std::uint8_t> add_message(std::uint64_t id) {
    std::vector<std::uint8_t> add(36);
    add[0]='A'; put(add, 5, 123456, 6); put(add, 11, id, 8); add[19]='B';
    put(add, 20, 100, 4);
    const char symbol[] = "AAPL    ";
    for (int i=0; i<8; ++i) add[24+i]=symbol[i];
    put(add, 32, 1892500, 4);
    return add;
}

std::vector<std::uint8_t> mold_packet(std::uint64_t sequence, const std::vector<std::uint8_t>& message) {
    std::vector<std::uint8_t> packet(22 + message.size());
    put(packet, 10, sequence, 8); put(packet, 18, 1, 2); put(packet, 20, message.size(), 2);
    std::copy(message.begin(), message.end(), packet.begin() + 22);
    return packet;
}

void test_pipeline() {
    using namespace marketcapture;
    std::vector<std::uint64_t> sequences;
    CapturePipeline pipeline([&](std::uint64_t sequence, const Event&) { sequences.push_back(sequence); });
    CHECK(pipeline.process(mold_packet(10, add_message(1))) == 1);
    CHECK(pipeline.process(mold_packet(12, add_message(2))) == 1);
    CHECK(sequences.size() == 2 && sequences[0] == 10 && sequences[1] == 12);
    CHECK(pipeline.metrics().packets() == 2);
    CHECK(pipeline.metrics().messages() == 2);
    CHECK(pipeline.metrics().gaps() == 1);
    CHECK(pipeline.next_sequence() == 13);
    throws([&] { (void)pipeline.process(mold_packet(11, add_message(3))); });
    pipeline.reset();
    CHECK(!pipeline.next_sequence());
}

void test_ring() {
    marketcapture::SpscRingBuffer<int, 4> queue;
    CHECK(queue.capacity() == 3);
    CHECK(queue.try_push(1)); CHECK(queue.try_push(2)); CHECK(queue.try_push(3));
    CHECK(!queue.try_push(4));
    CHECK(queue.try_pop() == 1); CHECK(queue.try_pop() == 2); CHECK(queue.try_pop() == 3);
    CHECK(!queue.try_pop().has_value()); CHECK(queue.empty());
}

void test_book() {
    using namespace marketcapture;
    OrderBook book;
    book.apply(AddOrder{1, 1, Side::buy, 100, "MSFT", 4000000});
    book.apply(AddOrder{2, 2, Side::buy, 50, "MSFT", 4000000});
    book.apply(AddOrder{3, 3, Side::sell, 80, "MSFT", 4000100});
    CHECK(book.best_bid()->shares == 150);
    CHECK(book.best_bid()->orders == 2);
    CHECK(book.best_ask()->price == 4000100);
    book.apply(ExecuteOrder{4, 1, 40, 9});
    book.apply(CancelOrder{5, 2, 50});
    CHECK(book.best_bid()->shares == 60);
    book.apply(DeleteOrder{6, 1});
    CHECK(!book.best_bid());
    CHECK(SnapshotManager{1}.capture(book, 6).asks.size() == 1);
    book.apply(AddOrderMpid{AddOrder{7, 4, Side::buy, 25, "MSFT", 3999900}, "ABCD"});
    book.apply(ReplaceOrder{8, 4, 5, 30, 3999950});
    CHECK(book.best_bid()->price == 3999950 && book.best_bid()->shares == 30);
    book.apply(ExecuteOrderPrice{ExecuteOrder{9, 5, 10, 10}, true, 3999950});
    CHECK(book.best_bid()->shares == 20);
    throws([&] { book.apply(DeleteOrder{7, 999}); });
}

void test_recording() {
    using namespace marketcapture;
    auto path = std::filesystem::temp_directory_path() / "marketcapture_test.ticks";
    {
        TickRecorder recorder(path);
        recorder.write(AddOrder{1, 7, Side::sell, 10, "NVDA", 100});
        recorder.write(ExecuteOrder{2, 7, 5, 88});
        recorder.write(CancelOrder{3, 7, 2});
        recorder.write(DeleteOrder{4, 7});
        recorder.write(AddOrderMpid{AddOrder{5, 8, Side::buy, 9, "NVDA", 101}, "TEST"});
        recorder.write(ReplaceOrder{6, 8, 9, 7, 102});
        recorder.write(ExecuteOrderPrice{ExecuteOrder{7, 9, 2, 99}, true, 102});
        recorder.flush();
    }
    std::vector<Event> events;
    CHECK(ReplayEngine{}.replay(path, [&](const Event& e) { events.push_back(e); }) == 7);
    CHECK(std::get<AddOrder>(events[0]).symbol == "NVDA");
    CHECK(std::get<ExecuteOrder>(events[1]).match_id == 88);
    CHECK(std::get<AddOrderMpid>(events[4]).attribution == "TEST");
    CHECK(std::get<ReplaceOrder>(events[5]).new_order_id == 9);
    CHECK(std::get<ExecuteOrderPrice>(events[6]).printable);
    std::filesystem::remove(path);
}

void test_metrics() {
    marketcapture::Metrics metrics;
    metrics.packet_received(); metrics.message_processed(); metrics.parse_error(); metrics.gap(3);
    CHECK(metrics.packets() == 1 && metrics.messages() == 1 && metrics.errors() == 1 && metrics.gaps() == 3);
    const auto elapsed = marketcapture::LatencyTimer{}.nanoseconds();
    CHECK(elapsed < 1'000'000'000);
}

void test_config() {
    const auto path = std::filesystem::temp_directory_path() / "marketcapture_feed_test.conf";
    {
        std::ofstream output(path);
        output << "multicast_group=233.1.2.3\nport=18000\n"
               << "interface_address=127.0.0.1\nreceive_buffer_bytes=1048576\n"
               << "record_path=test.ticks\npcap_path=test.pcap\nmax_packets=25\n";
    }
    const auto config = marketcapture::LiveFeedConfig::load(path);
    CHECK(config.multicast_group == "233.1.2.3");
    CHECK(config.port == 18000);
    CHECK(config.receive_buffer_bytes == 1048576);
    CHECK(config.max_packets == 25);
    CHECK(config.pcap_path == "test.pcap");
    std::filesystem::remove(path);
}

void test_malformed_prefixes() {
    const auto valid = add_message(77);
    for (std::size_t size = 0; size < valid.size(); ++size) {
        throws([&] {
            (void)marketcapture::ItchParser{}.parse(
                std::span<const std::uint8_t>(valid.data(), size));
        });
    }
    const auto packet = mold_packet(1, valid);
    for (std::size_t size = 0; size < packet.size(); ++size) {
        throws([&] {
            (void)marketcapture::MoldUdp64Decoder{}.decode(
                std::span<const std::uint8_t>(packet.data(), size));
        });
    }
}

void test_router_and_checkpoint() {
    using namespace marketcapture;
    ShardedBookRouter router(4);
    router.apply(AddOrder{1, 101, Side::buy, 10, "AAPL", 100});
    router.apply(AddOrder{2, 202, Side::sell, 20, "MSFT", 200});
    CHECK(router.symbol_count() == 2 && router.order_count() == 2);
    CHECK(router.find("AAPL")->best_bid()->shares == 10);
    router.apply(ReplaceOrder{3, 101, 102, 12, 101});
    CHECK(router.find("AAPL")->best_bid()->price == 101);

    const auto base = std::filesystem::temp_directory_path() / "marketcapture_checkpoint";
    CheckpointStore store(base);
    CHECK(store.save(router, 10).generation == 1);
    router.apply(AddOrder{4, 303, Side::buy, 5, "NVDA", 300});
    CHECK(store.save(router, 20).generation == 2);
    {
        std::ofstream corrupt(base.string() + ".a", std::ios::binary | std::ios::trunc);
        corrupt << "broken";
    }
    ShardedBookRouter restored(4);
    const auto loaded = store.load(restored);
    CHECK(loaded.generation == 1 && loaded.sequence == 10);
    CHECK(restored.order_count() == 2 && restored.find("NVDA") == nullptr);
    std::filesystem::remove(base.string() + ".a");
    std::filesystem::remove(base.string() + ".b");
}

void test_arbitration_and_pcap() {
    using namespace marketcapture;
    ExchangeSimulator simulator(42, {"AAPL"});
    const auto packets = simulator.generate(4);
    std::vector<std::uint64_t> emitted;
    std::vector<RecoveryRequest> requests;
    FeedArbitrator arbitrator(
        [&](const ArbitratedMessage& message) { emitted.push_back(message.sequence); },
        [&](const RecoveryRequest& request) { requests.push_back(request); }, 1);
    CHECK(arbitrator.ingest(FeedChannel::a, packets[0].datagram) == 1);
    CHECK(arbitrator.ingest(FeedChannel::a, packets[2].datagram) == 1);
    CHECK(emitted.size() == 1 && requests.size() == 1);
    CHECK(requests[0].first_sequence == 2 && requests[0].last_sequence == 2);
    CHECK(arbitrator.ingest(FeedChannel::b, packets[1].datagram) == 1);
    CHECK(emitted.size() == 3 && emitted[2] == 3);
    CHECK(arbitrator.ingest(FeedChannel::b, packets[0].datagram) == 0);
    CHECK(arbitrator.duplicates() == 1);

    const auto path = std::filesystem::temp_directory_path() / "marketcapture_test.pcap";
    {
        PcapWriter writer(path);
        writer.write(packets[0].datagram, 1'000'000'000);
        writer.write(packets[1].datagram, 1'000'001'000);
        writer.flush();
    }
    std::vector<std::vector<std::uint8_t>> replayed;
    CHECK(PcapReplay{}.replay(path, [&](std::uint64_t timestamp,
                                       std::span<const std::uint8_t> datagram) {
        CHECK(timestamp >= 1'000'000'000);
        replayed.emplace_back(datagram.begin(), datagram.end());
    }) == 2);
    CHECK(replayed[0] == packets[0].datagram && replayed[1] == packets[1].datagram);
    marketcapture::PcapRecoverySource recovery(path);
    std::vector<std::vector<std::uint8_t>> recovered;
    CHECK(recovery.recover(2, 2, [&](std::span<const std::uint8_t> datagram) {
        recovered.emplace_back(datagram.begin(), datagram.end());
    }) == 1);
    CHECK(recovered.front() == packets[1].datagram);
    std::filesystem::remove(path);
}

void test_simulator_and_latency() {
    marketcapture::ExchangeSimulator first(7);
    marketcapture::ExchangeSimulator second(7);
    for (int i = 0; i < 20; ++i) CHECK(first.next().datagram == second.next().datagram);
    const std::vector<std::uint64_t> samples{10, 20, 30, 40, 1000};
    const auto latency = marketcapture::summarize_latency(samples);
    CHECK(latency.minimum_ns == 10 && latency.p50_ns == 30);
    CHECK(latency.p99_ns == 1000 && latency.p999_ns == 1000);
}
}

int main() {
    test_itch(); test_mold(); test_pipeline(); test_ring(); test_book(); test_recording();
    test_metrics(); test_config(); test_router_and_checkpoint();
    test_arbitration_and_pcap(); test_simulator_and_latency(); test_malformed_prefixes();
    if (failures) return 1;
    std::cout << "All MarketCapture tests passed\n";
}
#include "marketcapture/config.hpp"
