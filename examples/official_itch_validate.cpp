#include "marketcapture/binary_file.hpp"
#include "marketcapture/hot_event.hpp"
#include "marketcapture/itch.hpp"
#include <array>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: marketcapture_validate_itch <decompressed-file|->\n";
        return 2;
    }
    try {
        std::array<std::uint64_t, 256> counts{};
        std::uint64_t order_ticks = 0;
        marketcapture::ItchParser reference;
        marketcapture::HotItchParser hot;
        const auto handler = [&](std::span<const std::uint8_t> message) {
            const auto full_event = reference.parse(message);
            const auto hot_event = hot.parse(message);
            ++counts[message[0]];
            order_ticks += hot_event.order_tick();
            if (marketcapture::is_order_tick(full_event) != hot_event.order_tick())
                throw std::runtime_error("hot/reference order classification mismatch");
        };
        marketcapture::BinaryFileReader reader;
        const auto stats = std::string_view(argv[1]) == "-"
            ? reader.read(std::cin, handler) : reader.read(argv[1], handler);
        std::cout << "messages=" << stats.messages << " bytes=" << stats.bytes
                  << " order_ticks=" << order_ticks << '\n';
        for (std::size_t type = 0; type < counts.size(); ++type)
            if (counts[type])
                std::cout << static_cast<char>(type) << '=' << counts[type] << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Validation failed: " << error.what() << '\n';
        return 1;
    }
}
