#include "marketcapture/feed_arbitrator.hpp"
#include <stdexcept>
#include <utility>

namespace marketcapture {

FeedArbitrator::FeedArbitrator(MessageHandler message_handler,
                               RecoveryHandler recovery_handler,
                               std::optional<std::uint64_t> initial_sequence,
                               std::size_t max_reorder_messages)
    : message_handler_(std::move(message_handler)),
      recovery_handler_(std::move(recovery_handler)),
      next_sequence_(initial_sequence),
      max_reorder_messages_(max_reorder_messages) {
    if (!message_handler_ || !recovery_handler_)
        throw std::invalid_argument("arbitrator handlers are required");
    if (max_reorder_messages_ == 0)
        throw std::invalid_argument("reorder buffer must be positive");
}

std::size_t FeedArbitrator::ingest(FeedChannel channel,
                                   std::span<const std::uint8_t> datagram) {
    const auto packet = decoder_.decode(datagram);
    if (!next_sequence_) next_sequence_ = packet.sequence;
    std::size_t accepted = 0;
    for (std::size_t index = 0; index < packet.messages.size(); ++index) {
        const auto sequence = packet.sequence + index;
        if (sequence < *next_sequence_) {
            ++duplicates_;
            continue;
        }
        ArbitratedMessage message{sequence, channel,
            std::vector<std::uint8_t>(packet.messages[index].begin(),
                                      packet.messages[index].end())};
        if (!buffered_.emplace(sequence, std::move(message)).second) {
            ++duplicates_;
            continue;
        }
        ++accepted;
        if (buffered_.size() > max_reorder_messages_)
            throw std::runtime_error("A/B reorder buffer capacity exceeded");
    }
    drain();
    return accepted;
}

void FeedArbitrator::drain() {
    while (next_sequence_) {
        auto found = buffered_.find(*next_sequence_);
        if (found == buffered_.end()) break;
        message_handler_(found->second);
        buffered_.erase(found);
        ++*next_sequence_;
        outstanding_request_.reset();
    }
    if (buffered_.empty() || !next_sequence_) return;
    const auto first_available = buffered_.begin()->first;
    if (first_available <= *next_sequence_) return;
    const RecoveryRequest missing{*next_sequence_, first_available - 1};
    if (!outstanding_request_ ||
        outstanding_request_->first_sequence != missing.first_sequence ||
        outstanding_request_->last_sequence != missing.last_sequence) {
        outstanding_request_ = missing;
        ++recovery_requests_;
        recovery_handler_(missing);
    }
}

} // namespace marketcapture
