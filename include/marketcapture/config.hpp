#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace marketcapture {

struct LiveFeedConfig {
    std::string multicast_group;
    std::uint16_t port{};
    std::string interface_address{"0.0.0.0"};
    int receive_buffer_bytes{8 * 1024 * 1024};
    std::filesystem::path record_path;
    std::uint64_t max_packets{}; // zero means unlimited

    [[nodiscard]] static LiveFeedConfig load(const std::filesystem::path& path);
    void validate() const;
};

} // namespace marketcapture
