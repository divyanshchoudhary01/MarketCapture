#include "marketcapture/recorder.hpp"
#include <array>
#include <stdexcept>
#include <type_traits>

namespace marketcapture {
namespace {
constexpr std::array<char, 8> magic{'M','C','A','P','T','I','K','1'};
template <typename T> void write_value(std::ostream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
template <typename T> T read_value(std::istream& in) {
    T value{};
    if (!in.read(reinterpret_cast<char*>(&value), sizeof(value))) throw std::runtime_error("truncated tick file");
    return value;
}
void write_string(std::ostream& out, const std::string& value) {
    if (value.size() > 255) throw std::invalid_argument("string too long");
    write_value(out, static_cast<std::uint8_t>(value.size()));
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
}
std::string read_string(std::istream& in) {
    const auto length = read_value<std::uint8_t>(in);
    std::string value(length, '\0');
    if (!in.read(value.data(), length)) throw std::runtime_error("truncated tick file");
    return value;
}
}

TickRecorder::TickRecorder(const std::filesystem::path& path)
    : output_(path, std::ios::binary | std::ios::trunc) {
    if (!output_) throw std::runtime_error("cannot open tick file");
    output_.write(magic.data(), magic.size());
}

void TickRecorder::write(const Event& event) {
    std::visit([this](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, AddOrder>) {
            write_value(output_, std::uint8_t{'A'}); write_value(output_, v.timestamp);
            write_value(output_, v.order_id); write_value(output_, static_cast<std::uint8_t>(v.side));
            write_value(output_, v.shares); write_value(output_, v.price);
            write_string(output_, v.symbol);
        } else if constexpr (std::is_same_v<T, AddOrderMpid>) {
            const auto& o = v.order;
            write_value(output_, std::uint8_t{'F'}); write_value(output_, o.timestamp);
            write_value(output_, o.order_id); write_value(output_, static_cast<std::uint8_t>(o.side));
            write_value(output_, o.shares); write_value(output_, o.price);
            write_string(output_, o.symbol); write_string(output_, v.attribution);
        } else if constexpr (std::is_same_v<T, ExecuteOrder>) {
            write_value(output_, std::uint8_t{'E'}); write_value(output_, v.timestamp);
            write_value(output_, v.order_id); write_value(output_, v.shares); write_value(output_, v.match_id);
        } else if constexpr (std::is_same_v<T, ExecuteOrderPrice>) {
            const auto& e = v.execution;
            write_value(output_, std::uint8_t{'C'}); write_value(output_, e.timestamp);
            write_value(output_, e.order_id); write_value(output_, e.shares); write_value(output_, e.match_id);
            write_value(output_, static_cast<std::uint8_t>(v.printable)); write_value(output_, v.price);
        } else if constexpr (std::is_same_v<T, CancelOrder>) {
            write_value(output_, std::uint8_t{'X'}); write_value(output_, v.timestamp);
            write_value(output_, v.order_id); write_value(output_, v.shares);
        } else if constexpr (std::is_same_v<T, DeleteOrder>) {
            write_value(output_, std::uint8_t{'D'}); write_value(output_, v.timestamp);
            write_value(output_, v.order_id);
        } else if constexpr (std::is_same_v<T, ReplaceOrder>) {
            write_value(output_, std::uint8_t{'U'}); write_value(output_, v.timestamp);
            write_value(output_, v.original_order_id); write_value(output_, v.new_order_id);
            write_value(output_, v.shares); write_value(output_, v.price);
        } else {
            throw std::invalid_argument("event is informational and not recordable as an order tick");
        }
    }, event);
    if (!output_) throw std::runtime_error("failed writing tick file");
}
void TickRecorder::flush() { output_.flush(); }

std::size_t ReplayEngine::replay(const std::filesystem::path& path, const Handler& handler) const {
    std::ifstream in(path, std::ios::binary);
    std::array<char, 8> header{};
    if (!in.read(header.data(), header.size()) || header != magic) throw std::runtime_error("invalid tick file");
    std::size_t count = 0;
    while (in.peek() != std::char_traits<char>::eof()) {
        const auto type = read_value<std::uint8_t>(in);
        const auto timestamp = read_value<std::uint64_t>(in);
        const auto id = read_value<std::uint64_t>(in);
        Event event;
        switch (type) {
        case 'A': {
            const auto side = read_value<std::uint8_t>(in);
            if (side != 'B' && side != 'S') throw std::runtime_error("invalid recorded side");
            const auto shares = read_value<std::uint32_t>(in);
            const auto price = read_value<std::uint32_t>(in);
            auto symbol = read_string(in);
            event = AddOrder{timestamp, id, static_cast<Side>(side), shares, std::move(symbol), price};
            break;
        }
        case 'F': {
            const auto side = read_value<std::uint8_t>(in);
            if (side != 'B' && side != 'S') throw std::runtime_error("invalid recorded side");
            const auto shares = read_value<std::uint32_t>(in);
            const auto price = read_value<std::uint32_t>(in);
            auto symbol = read_string(in);
            auto attribution = read_string(in);
            event = AddOrderMpid{AddOrder{timestamp, id, static_cast<Side>(side), shares,
                std::move(symbol), price}, std::move(attribution)};
            break;
        }
        case 'E': event = ExecuteOrder{timestamp, id, read_value<std::uint32_t>(in), read_value<std::uint64_t>(in)}; break;
        case 'C': {
            ExecuteOrder execution{timestamp, id, read_value<std::uint32_t>(in), read_value<std::uint64_t>(in)};
            const auto printable = read_value<std::uint8_t>(in);
            if (printable > 1) throw std::runtime_error("invalid recorded printable flag");
            event = ExecuteOrderPrice{execution, printable != 0, read_value<std::uint32_t>(in)};
            break;
        }
        case 'X': event = CancelOrder{timestamp, id, read_value<std::uint32_t>(in)}; break;
        case 'D': event = DeleteOrder{timestamp, id}; break;
        case 'U': event = ReplaceOrder{timestamp, id, read_value<std::uint64_t>(in),
            read_value<std::uint32_t>(in), read_value<std::uint32_t>(in)}; break;
        default: throw std::runtime_error("unknown tick record type");
        }
        handler(event);
        ++count;
    }
    return count;
}
} // namespace marketcapture
