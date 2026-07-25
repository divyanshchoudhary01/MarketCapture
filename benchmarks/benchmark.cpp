#include "marketcapture/itch.hpp"
#include "marketcapture/pipeline.hpp"
#include "marketcapture/ring_buffer.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {
void put(std::vector<std::uint8_t>& value, std::size_t position,
         std::uint64_t number, std::size_t bytes) {
    for (std::size_t i = 0; i < bytes; ++i)
        value[position + bytes - 1 - i] = static_cast<std::uint8_t>(number >> (i * 8));
}
}

int main(int argc, char** argv) {
    const std::uint64_t iterations = argc > 1 ? std::stoull(argv[1]) : 1'000'000;
    std::vector<std::uint8_t> message(36, ' ');
    message[0] = 'A'; message[19] = 'B';
    put(message, 11, 42, 8); put(message, 20, 100, 4); put(message, 32, 1234500, 4);

    marketcapture::ItchParser parser;
    auto start = std::chrono::steady_clock::now();
    for (std::uint64_t i = 0; i < iterations; ++i) {
        auto event = parser.parse(message);
        if (std::get<marketcapture::AddOrder>(event).order_id != 42) return 2;
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    const auto parse_ns = std::chrono::duration<double, std::nano>(elapsed).count() / iterations;

    marketcapture::SpscRingBuffer<std::uint64_t, 1024> queue;
    start = std::chrono::steady_clock::now();
    for (std::uint64_t i = 0; i < iterations; ++i) {
        if (!queue.try_push(i) || !queue.try_pop()) return 3;
    }
    elapsed = std::chrono::steady_clock::now() - start;
    const auto roundtrip_ns = std::chrono::duration<double, std::nano>(elapsed).count() / iterations;

    std::vector<std::uint8_t> packet(22 + message.size());
    put(packet, 18, 1, 2);
    put(packet, 20, message.size(), 2);
    std::copy(message.begin(), message.end(), packet.begin() + 22);
    std::uint64_t delivered = 0;
    marketcapture::CapturePipeline pipeline(
        [&](std::uint64_t, const marketcapture::Event&) { ++delivered; });
    start = std::chrono::steady_clock::now();
    for (std::uint64_t i = 0; i < iterations; ++i) {
        put(packet, 10, i + 1, 8);
        if (pipeline.process(packet) != 1) return 4;
    }
    elapsed = std::chrono::steady_clock::now() - start;
    if (delivered != iterations) return 5;
    const auto pipeline_ns = std::chrono::duration<double, std::nano>(elapsed).count() / iterations;

    std::cout << "iterations=" << iterations
              << " itch_parse_ns=" << parse_ns
              << " spsc_roundtrip_ns=" << roundtrip_ns
              << " mold_to_event_ns=" << pipeline_ns
              << " mold_to_event_mps=" << 1'000'000'000.0 / pipeline_ns << '\n';
}
