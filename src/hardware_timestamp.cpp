#include "marketcapture/hardware_timestamp.hpp"
#include <stdexcept>
#ifdef __linux__
#include <arpa/inet.h>
#include <array>
#include <cstring>
#include <linux/net_tstamp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace marketcapture {

HardwareTimestampReceiver::HardwareTimestampReceiver(
    std::string group, std::uint16_t port, std::string interface_address,
    int receive_buffer_bytes) {
#ifdef __linux__
    socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ < 0) throw std::runtime_error("hardware timestamp socket failed");
    const auto fail = [&](const char* message) {
        ::close(socket_); socket_ = -1; throw std::runtime_error(message);
    };
    int reuse = 1;
    if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0 ||
        setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
                   &receive_buffer_bytes, sizeof(receive_buffer_bytes)) != 0)
        fail("hardware timestamp socket options failed");
    const int timestamp_flags = SOF_TIMESTAMPING_RX_HARDWARE |
        SOF_TIMESTAMPING_RAW_HARDWARE | SOF_TIMESTAMPING_RX_SOFTWARE |
        SOF_TIMESTAMPING_SOFTWARE;
    if (setsockopt(socket_, SOL_SOCKET, SO_TIMESTAMPING,
                   &timestamp_flags, sizeof(timestamp_flags)) != 0)
        fail("SO_TIMESTAMPING is unavailable");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
        fail("hardware timestamp UDP bind failed");
    ip_mreq membership{};
    if (inet_pton(AF_INET, group.c_str(), &membership.imr_multiaddr) != 1 ||
        inet_pton(AF_INET, interface_address.c_str(), &membership.imr_interface) != 1)
        fail("invalid hardware timestamp multicast address");
    if (setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &membership, sizeof(membership)) != 0)
        fail("hardware timestamp multicast join failed");
#else
    (void)group; (void)port; (void)interface_address; (void)receive_buffer_bytes;
    throw std::runtime_error("NIC hardware timestamp receiver requires Linux");
#endif
}

HardwareTimestampReceiver::~HardwareTimestampReceiver() {
#ifdef __linux__
    if (socket_ >= 0) ::close(socket_);
#endif
}

TimestampedDatagram HardwareTimestampReceiver::receive() {
#ifdef __linux__
    TimestampedDatagram result;
    result.payload.resize(65536);
    std::array<std::uint8_t, 512> control{};
    iovec vector{result.payload.data(), result.payload.size()};
    msghdr message{};
    message.msg_iov = &vector;
    message.msg_iovlen = 1;
    message.msg_control = control.data();
    message.msg_controllen = control.size();
    const auto received = recvmsg(socket_, &message, 0);
    if (received < 0) throw std::runtime_error("timestamped receive failed");
    result.payload.resize(static_cast<std::size_t>(received));
    for (auto* header = CMSG_FIRSTHDR(&message); header;
         header = CMSG_NXTHDR(&message, header)) {
        if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_TIMESTAMPING)
            continue;
        const auto* timestamps = reinterpret_cast<const timespec*>(CMSG_DATA(header));
        const timespec* selected = nullptr;
        if (timestamps[2].tv_sec != 0 || timestamps[2].tv_nsec != 0) {
            selected = &timestamps[2];
            result.quality = TimestampQuality::hardware;
        } else if (timestamps[0].tv_sec != 0 || timestamps[0].tv_nsec != 0) {
            selected = &timestamps[0];
            result.quality = TimestampQuality::software;
        }
        if (selected) {
            result.timestamp_ns = std::uint64_t(selected->tv_sec) * 1'000'000'000ull +
                                  static_cast<std::uint64_t>(selected->tv_nsec);
        }
    }
    return result;
#else
    throw std::runtime_error("NIC hardware timestamp receiver requires Linux");
#endif
}

} // namespace marketcapture
