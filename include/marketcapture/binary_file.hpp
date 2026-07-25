#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <istream>
#include <span>

namespace marketcapture {

struct BinaryFileStats {
    std::uint64_t messages{};
    std::uint64_t bytes{};
};

class BinaryFileReader {
public:
    using Handler = std::function<void(std::span<const std::uint8_t>)>;
    [[nodiscard]] BinaryFileStats read(std::istream& input,
                                       const Handler& handler) const;
    [[nodiscard]] BinaryFileStats read(const std::filesystem::path& path,
                                       const Handler& handler) const;
};

} // namespace marketcapture
