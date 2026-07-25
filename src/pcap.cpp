#include "marketcapture/pcap.hpp"
#include "marketcapture/moldudp64.hpp"
#include <array>
#include <limits>
#include <stdexcept>
#include <vector>

namespace marketcapture {
namespace {
template <typename T> void write_value(std::ostream& output, T value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
template <typename T> T read_value(std::istream& input) {
    T value{};
    if (!input.read(reinterpret_cast<char*>(&value), sizeof(value)))
        throw std::runtime_error("truncated PCAP");
    return value;
}
}

PcapWriter::PcapWriter(const std::filesystem::path& path)
    : output_(path, std::ios::binary | std::ios::trunc) {
    if (!output_) throw std::runtime_error("cannot create PCAP file");
    write_value(output_, std::uint32_t{0xa1b2c3d4});
    write_value(output_, std::uint16_t{2});
    write_value(output_, std::uint16_t{4});
    write_value(output_, std::int32_t{0});
    write_value(output_, std::uint32_t{0});
    write_value(output_, std::uint32_t{65535});
    write_value(output_, std::uint32_t{147}); // DLT_USER0: raw MoldUDP64 payload
}

void PcapWriter::write(std::span<const std::uint8_t> datagram,
                       std::uint64_t timestamp_ns) {
    if (datagram.size() > 65535 ||
        datagram.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument("PCAP datagram is too large");
    const auto seconds = static_cast<std::uint32_t>(timestamp_ns / 1'000'000'000ull);
    const auto microseconds = static_cast<std::uint32_t>((timestamp_ns % 1'000'000'000ull) / 1000);
    const auto size = static_cast<std::uint32_t>(datagram.size());
    write_value(output_, seconds); write_value(output_, microseconds);
    write_value(output_, size); write_value(output_, size);
    output_.write(reinterpret_cast<const char*>(datagram.data()),
                  static_cast<std::streamsize>(datagram.size()));
    if (!output_) throw std::runtime_error("PCAP write failed");
}

void PcapWriter::flush() { output_.flush(); }

std::size_t PcapReplay::replay(const std::filesystem::path& path,
                               const Handler& handler) const {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open PCAP file");
    if (read_value<std::uint32_t>(input) != 0xa1b2c3d4 ||
        read_value<std::uint16_t>(input) != 2 ||
        read_value<std::uint16_t>(input) != 4)
        throw std::runtime_error("unsupported PCAP header");
    (void)read_value<std::int32_t>(input);
    (void)read_value<std::uint32_t>(input);
    const auto snap_length = read_value<std::uint32_t>(input);
    if (read_value<std::uint32_t>(input) != 147)
        throw std::runtime_error("PCAP does not contain raw MoldUDP64 payloads");
    std::size_t count = 0;
    while (input.peek() != std::char_traits<char>::eof()) {
        const auto seconds = read_value<std::uint32_t>(input);
        const auto microseconds = read_value<std::uint32_t>(input);
        const auto included = read_value<std::uint32_t>(input);
        const auto original = read_value<std::uint32_t>(input);
        if (included != original || included > snap_length)
            throw std::runtime_error("invalid or truncated PCAP record");
        std::vector<std::uint8_t> datagram(included);
        if (!input.read(reinterpret_cast<char*>(datagram.data()), included))
            throw std::runtime_error("truncated PCAP record payload");
        handler(std::uint64_t(seconds) * 1'000'000'000ull +
                std::uint64_t(microseconds) * 1000ull, datagram);
        ++count;
    }
    return count;
}

PcapRecoverySource::PcapRecoverySource(const std::filesystem::path& path) {
    MoldUdp64Decoder decoder;
    (void)PcapReplay{}.replay(path,
        [&](std::uint64_t, std::span<const std::uint8_t> datagram) {
            const auto packet = decoder.decode(datagram);
            if (packet.messages.empty()) return;
            entries_.push_back({packet.sequence,
                packet.sequence + packet.messages.size() - 1,
                std::vector<std::uint8_t>(datagram.begin(), datagram.end())});
        });
}

std::size_t PcapRecoverySource::recover(std::uint64_t first_sequence,
                                        std::uint64_t last_sequence,
                                        const Handler& handler) const {
    if (first_sequence > last_sequence)
        throw std::invalid_argument("invalid recovery range");
    std::size_t delivered = 0;
    for (const auto& entry : entries_) {
        if (entry.last < first_sequence) continue;
        if (entry.first > last_sequence) break;
        handler(entry.datagram);
        ++delivered;
    }
    return delivered;
}

} // namespace marketcapture
