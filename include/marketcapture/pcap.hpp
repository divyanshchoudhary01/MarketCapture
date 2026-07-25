#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <span>
#include <vector>

namespace marketcapture {

class PcapWriter {
public:
    explicit PcapWriter(const std::filesystem::path& path);
    void write(std::span<const std::uint8_t> datagram, std::uint64_t timestamp_ns);
    void flush();
private:
    std::ofstream output_;
};

class PcapReplay {
public:
    using Handler = std::function<void(std::uint64_t timestamp_ns,
                                       std::span<const std::uint8_t> datagram)>;
    [[nodiscard]] std::size_t replay(const std::filesystem::path& path,
                                     const Handler& handler) const;
};

class PcapRecoverySource {
public:
    using Handler = std::function<void(std::span<const std::uint8_t> datagram)>;
    explicit PcapRecoverySource(const std::filesystem::path& path);
    [[nodiscard]] std::size_t recover(std::uint64_t first_sequence,
                                      std::uint64_t last_sequence,
                                      const Handler& handler) const;
private:
    struct Entry {
        std::uint64_t first{};
        std::uint64_t last{};
        std::vector<std::uint8_t> datagram;
    };
    std::vector<Entry> entries_;
};

} // namespace marketcapture
