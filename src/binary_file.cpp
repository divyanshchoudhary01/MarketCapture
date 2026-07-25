#include "marketcapture/binary_file.hpp"
#include <array>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace marketcapture {

BinaryFileStats BinaryFileReader::read(std::istream& input,
                                       const Handler& handler) const {
    BinaryFileStats stats;
    std::array<std::uint8_t, 2> length_bytes{};
    std::vector<std::uint8_t> message(65535);
    while (input.read(reinterpret_cast<char*>(length_bytes.data()), 2)) {
        const auto length = (std::uint16_t(length_bytes[0]) << 8) | length_bytes[1];
        if (length == 0) throw std::runtime_error("zero-length BinaryFile record");
        if (!input.read(reinterpret_cast<char*>(message.data()), length))
            throw std::runtime_error("truncated BinaryFile record");
        handler(std::span<const std::uint8_t>(message.data(), length));
        ++stats.messages;
        stats.bytes += length;
    }
    if (!input.eof()) throw std::runtime_error("truncated BinaryFile length");
    return stats;
}

BinaryFileStats BinaryFileReader::read(const std::filesystem::path& path,
                                       const Handler& handler) const {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open BinaryFile input");
    return read(input, handler);
}

} // namespace marketcapture
