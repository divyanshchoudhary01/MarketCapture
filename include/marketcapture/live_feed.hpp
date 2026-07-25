#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace marketcapture {

class UdpLiveFeed {
public:
    using PacketHandler = std::function<void(std::span<const std::uint8_t>)>;
    UdpLiveFeed(std::string group, std::uint16_t port,
                std::string interface_address = "0.0.0.0",
                int receive_buffer_bytes = 8 * 1024 * 1024);
    ~UdpLiveFeed();
    UdpLiveFeed(const UdpLiveFeed&) = delete;
    UdpLiveFeed& operator=(const UdpLiveFeed&) = delete;
    void run(const PacketHandler& handler);
    void stop() noexcept;
private:
    std::string group_, interface_;
    std::uint16_t port_;
    int receive_buffer_bytes_;
    std::atomic<bool> running_{false};
    std::intptr_t socket_{-1};
};

} // namespace marketcapture
