#include "marketcapture/live_feed.hpp"
#include <array>
#include <cerrno>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace marketcapture {
namespace {
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
void close_socket(Socket s) { closesocket(s); }
#else
using Socket = int;
constexpr Socket invalid_socket = -1;
void close_socket(Socket s) { close(s); }
#endif
}

UdpLiveFeed::UdpLiveFeed(std::string group, std::uint16_t port,
                         std::string interface_address, int receive_buffer_bytes)
    : group_(std::move(group)), interface_(std::move(interface_address)), port_(port),
      receive_buffer_bytes_(receive_buffer_bytes) {
    if (port_ == 0 || receive_buffer_bytes_ < 64 * 1024)
        throw std::invalid_argument("invalid UDP port or receive buffer size");
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("WSAStartup failed");
#endif
}
UdpLiveFeed::~UdpLiveFeed() {
    stop();
    if (socket_ != -1) close_socket(static_cast<Socket>(socket_));
#ifdef _WIN32
    WSACleanup();
#endif
}

void UdpLiveFeed::stop() noexcept {
    running_.store(false);
    if (socket_ == -1) return;
#ifdef _WIN32
    shutdown(static_cast<Socket>(socket_), SD_BOTH);
#else
    shutdown(static_cast<Socket>(socket_), SHUT_RDWR);
#endif
}

void UdpLiveFeed::run(const PacketHandler& handler) {
    Socket s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == invalid_socket) throw std::runtime_error("UDP socket creation failed");
    socket_ = static_cast<std::intptr_t>(s);
    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
                   reinterpret_cast<const char*>(&receive_buffer_bytes_),
                   sizeof(receive_buffer_bytes_)) != 0) {
        close_socket(s);
        socket_ = -1;
        throw std::runtime_error("failed to configure UDP receive buffer");
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close_socket(s);
        socket_ = -1;
        throw std::runtime_error("UDP bind failed");
    }
    ip_mreq membership{};
    if (inet_pton(AF_INET, group_.c_str(), &membership.imr_multiaddr) != 1 ||
        inet_pton(AF_INET, interface_.c_str(), &membership.imr_interface) != 1) {
        close_socket(s);
        socket_ = -1;
        throw std::invalid_argument("invalid multicast or interface address");
    }
    if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   reinterpret_cast<const char*>(&membership), sizeof(membership)) != 0) {
        close_socket(s);
        socket_ = -1;
        throw std::runtime_error("multicast join failed");
    }
    std::array<std::uint8_t, 65536> buffer{};
    running_.store(true);
    while (running_.load()) {
#ifdef _WIN32
        const int received = recv(s, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
#else
        const auto received = recv(s, buffer.data(), buffer.size(), 0);
#endif
        if (received < 0) {
            if (!running_.load()) break;
            throw std::runtime_error("UDP receive failed");
        }
        handler(std::span<const std::uint8_t>(buffer.data(), static_cast<std::size_t>(received)));
    }
    close_socket(s);
    socket_ = -1;
}
} // namespace marketcapture
