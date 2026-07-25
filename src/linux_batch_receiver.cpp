#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "marketcapture/linux_batch_receiver.hpp"
#include <stdexcept>
#include <vector>
#ifdef __linux__
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#ifdef MARKETCAPTURE_HAS_IO_URING
#include <liburing.h>
#endif
#endif

namespace marketcapture {

class RecvmmsgReceiver::Impl {
public:
    Impl(int descriptor, std::size_t batch, std::size_t size)
        : socket_fd(descriptor), buffers(batch, std::vector<std::uint8_t>(size)) {
        if (descriptor < 0 || batch == 0 || size == 0)
            throw std::invalid_argument("invalid recvmmsg receiver configuration");
#ifdef __linux__
        vectors.resize(batch);
        messages.resize(batch);
        for (std::size_t index = 0; index < batch; ++index) {
            vectors[index] = {buffers[index].data(), buffers[index].size()};
            messages[index] = {};
            messages[index].msg_hdr.msg_iov = &vectors[index];
            messages[index].msg_hdr.msg_iovlen = 1;
        }
#endif
    }
    int socket_fd;
    std::vector<std::vector<std::uint8_t>> buffers;
#ifdef __linux__
    std::vector<iovec> vectors;
    std::vector<mmsghdr> messages;
#endif
};

RecvmmsgReceiver::RecvmmsgReceiver(int socket_fd, std::size_t batch_size,
                                   std::size_t datagram_size)
    : impl_(std::make_unique<Impl>(socket_fd, batch_size, datagram_size)) {
#ifndef __linux__
    throw std::runtime_error("recvmmsg receiver requires Linux");
#endif
}
RecvmmsgReceiver::~RecvmmsgReceiver() = default;

std::size_t RecvmmsgReceiver::receive_batch(const Handler& handler) {
#ifdef __linux__
    for (auto& message : impl_->messages) message.msg_len = 0;
    const auto received = recvmmsg(impl_->socket_fd, impl_->messages.data(),
        static_cast<unsigned>(impl_->messages.size()), MSG_WAITFORONE, nullptr);
    if (received < 0) throw std::runtime_error(
        std::string("recvmmsg failed: ") + std::strerror(errno));
    for (int index = 0; index < received; ++index)
        handler(std::span<const std::uint8_t>(impl_->buffers[index].data(),
            impl_->messages[index].msg_len));
    return static_cast<std::size_t>(received);
#else
    (void)handler;
    throw std::runtime_error("recvmmsg receiver requires Linux");
#endif
}

class IoUringUdpReceiver::Impl {
public:
    Impl(int descriptor, std::size_t depth, std::size_t size)
        : socket_fd(descriptor), buffers(depth, std::vector<std::uint8_t>(size)) {
        if (descriptor < 0 || depth == 0 || size == 0)
            throw std::invalid_argument("invalid io_uring receiver configuration");
#ifdef MARKETCAPTURE_HAS_IO_URING
        if (io_uring_queue_init(static_cast<unsigned>(depth), &ring, 0) < 0)
            throw std::runtime_error("io_uring queue initialization failed");
        initialized = true;
        for (std::size_t index = 0; index < buffers.size(); ++index) arm(index);
        if (io_uring_submit(&ring) < 0) throw std::runtime_error("io_uring submit failed");
#else
        throw std::runtime_error("MarketCapture was built without io_uring");
#endif
    }
    ~Impl() {
#ifdef MARKETCAPTURE_HAS_IO_URING
        if (initialized) io_uring_queue_exit(&ring);
#endif
    }
#ifdef MARKETCAPTURE_HAS_IO_URING
    void arm(std::size_t index) {
        auto* entry = io_uring_get_sqe(&ring);
        if (!entry) throw std::runtime_error("io_uring submission queue is full");
        io_uring_prep_recv(entry, socket_fd, buffers[index].data(), buffers[index].size(), 0);
        io_uring_sqe_set_data64(entry, index);
    }
    io_uring ring{};
    bool initialized{};
#endif
    int socket_fd;
    std::vector<std::vector<std::uint8_t>> buffers;
};

IoUringUdpReceiver::IoUringUdpReceiver(int socket_fd, std::size_t queue_depth,
                                       std::size_t datagram_size)
    : impl_(std::make_unique<Impl>(socket_fd, queue_depth, datagram_size)) {}
IoUringUdpReceiver::~IoUringUdpReceiver() = default;

std::size_t IoUringUdpReceiver::receive_batch(const Handler& handler) {
#ifdef MARKETCAPTURE_HAS_IO_URING
    io_uring_cqe* first{};
    const auto waited = io_uring_wait_cqe(&impl_->ring, &first);
    if (waited < 0) throw std::runtime_error("io_uring wait failed");
    std::vector<io_uring_cqe*> completions(impl_->buffers.size());
    const auto count = io_uring_peek_batch_cqe(
        &impl_->ring, completions.data(), static_cast<unsigned>(completions.size()));
    std::size_t delivered = 0;
    for (unsigned index = 0; index < count; ++index) {
        auto* completion = completions[index];
        const auto buffer_id = static_cast<std::size_t>(io_uring_cqe_get_data64(completion));
        if (completion->res < 0)
            throw std::runtime_error("io_uring receive completion failed");
        if (completion->res > 0) {
            handler(std::span<const std::uint8_t>(impl_->buffers[buffer_id].data(),
                static_cast<std::size_t>(completion->res)));
            ++delivered;
        }
        impl_->arm(buffer_id);
    }
    io_uring_cq_advance(&impl_->ring, count);
    if (io_uring_submit(&impl_->ring) < 0) throw std::runtime_error("io_uring resubmit failed");
    return delivered;
#else
    (void)handler;
    throw std::runtime_error("MarketCapture was built without io_uring");
#endif
}

} // namespace marketcapture
