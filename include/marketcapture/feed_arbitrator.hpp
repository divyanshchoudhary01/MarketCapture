#pragma once

#include "marketcapture/moldudp64.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace marketcapture {

enum class FeedChannel { a, b, recovery };

struct ArbitratedMessage {
    std::uint64_t sequence{};
    FeedChannel channel{};
    std::vector<std::uint8_t> payload;
};

struct RecoveryRequest {
    std::uint64_t first_sequence{};
    std::uint64_t last_sequence{};
};

class FeedArbitrator {
public:
    using MessageHandler = std::function<void(const ArbitratedMessage&)>;
    using RecoveryHandler = std::function<void(const RecoveryRequest&)>;

    FeedArbitrator(MessageHandler message_handler, RecoveryHandler recovery_handler,
                   std::optional<std::uint64_t> initial_sequence = std::nullopt,
                   std::size_t max_reorder_messages = 65536);

    std::size_t ingest(FeedChannel channel, std::span<const std::uint8_t> datagram);
    [[nodiscard]] std::optional<std::uint64_t> next_sequence() const noexcept {
        return next_sequence_;
    }
    [[nodiscard]] std::size_t buffered() const noexcept { return buffered_.size(); }
    [[nodiscard]] std::uint64_t duplicates() const noexcept { return duplicates_; }
    [[nodiscard]] std::uint64_t recovery_requests() const noexcept {
        return recovery_requests_;
    }

private:
    void drain();
    MoldUdp64Decoder decoder_;
    MessageHandler message_handler_;
    RecoveryHandler recovery_handler_;
    std::optional<std::uint64_t> next_sequence_;
    std::optional<RecoveryRequest> outstanding_request_;
    std::map<std::uint64_t, ArbitratedMessage> buffered_;
    std::size_t max_reorder_messages_;
    std::uint64_t duplicates_{};
    std::uint64_t recovery_requests_{};
};

} // namespace marketcapture
