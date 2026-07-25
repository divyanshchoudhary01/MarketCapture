#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace marketcapture {

enum class TimestampQuality { unavailable, software, hardware };

struct TimestampedDatagram {
    std::vector<std::uint8_t> payload;
    std::uint64_t timestamp_ns{};
    TimestampQuality quality{TimestampQuality::unavailable};
};

class HardwareTimestampReceiver {
public:
    HardwareTimestampReceiver(std::string group, std::uint16_t port,
                              std::string interface_address,
                              int receive_buffer_bytes = 16 * 1024 * 1024);
    ~HardwareTimestampReceiver();
    HardwareTimestampReceiver(const HardwareTimestampReceiver&) = delete;
    HardwareTimestampReceiver& operator=(const HardwareTimestampReceiver&) = delete;

    [[nodiscard]] TimestampedDatagram receive();
    [[nodiscard]] static constexpr bool platform_supported() noexcept {
#ifdef __linux__
        return true;
#else
        return false;
#endif
    }

private:
    int socket_{-1};
};

} // namespace marketcapture
