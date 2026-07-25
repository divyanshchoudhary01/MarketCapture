#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace marketcapture {

class RecvmmsgReceiver {
public:
    using Handler = std::function<void(std::span<const std::uint8_t>)>;
    RecvmmsgReceiver(int socket_fd, std::size_t batch_size = 32,
                     std::size_t datagram_size = 65536);
    ~RecvmmsgReceiver();
    [[nodiscard]] std::size_t receive_batch(const Handler& handler);
    [[nodiscard]] static constexpr bool platform_supported() noexcept {
#ifdef __linux__
        return true;
#else
        return false;
#endif
    }
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class IoUringUdpReceiver {
public:
    using Handler = std::function<void(std::span<const std::uint8_t>)>;
    IoUringUdpReceiver(int socket_fd, std::size_t queue_depth = 64,
                       std::size_t datagram_size = 65536);
    ~IoUringUdpReceiver();
    [[nodiscard]] std::size_t receive_batch(const Handler& handler);
    [[nodiscard]] static constexpr bool compiled() noexcept {
#ifdef MARKETCAPTURE_HAS_IO_URING
        return true;
#else
        return false;
#endif
    }
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace marketcapture
