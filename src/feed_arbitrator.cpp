#include "marketcapture/feed_arbitrator.hpp"
#include <algorithm>
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
      reorder_slots_(max_reorder_messages),
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
        if (packet.messages[index].size() > ArbitratedMessage{}.storage.size())
            throw std::runtime_error("ITCH message exceeds fixed arbitration slot");
        ArbitratedMessage message;
        message.sequence = sequence;
        message.channel = channel;
        message.size = static_cast<std::uint8_t>(packet.messages[index].size());
        std::copy(packet.messages[index].begin(), packet.messages[index].end(),
                  message.storage.begin());
        auto& slot = reorder_slots_[sequence % max_reorder_messages_];
        if (slot.occupied && slot.sequence == sequence) {
            ++duplicates_;
            continue;
        }
        if (slot.occupied)
            throw std::runtime_error("A/B reorder window capacity exceeded");
        slot.occupied = true;
        slot.sequence = sequence;
        slot.message = std::move(message);
        ++buffered_count_;
        if (!lowest_buffered_ || sequence < *lowest_buffered_)
            lowest_buffered_ = sequence;
        ++accepted;
    }
    drain();
    return accepted;
}

void FeedArbitrator::drain() {
    while (next_sequence_) {
        auto& slot = reorder_slots_[*next_sequence_ % max_reorder_messages_];
        if (!slot.occupied || slot.sequence != *next_sequence_) break;
        message_handler_(slot.message);
        slot.occupied = false;
        --buffered_count_;
        if (buffered_count_ == 0) lowest_buffered_.reset();
        else if (lowest_buffered_ == *next_sequence_) refresh_lowest();
        ++*next_sequence_;
        outstanding_request_.reset();
    }
    if (buffered_count_ == 0 || !next_sequence_ || !lowest_buffered_) return;
    const auto first_available = *lowest_buffered_;
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

void FeedArbitrator::refresh_lowest() {
    lowest_buffered_.reset();
    for (const auto& slot : reorder_slots_)
        if (slot.occupied && (!lowest_buffered_ || slot.sequence < *lowest_buffered_))
            lowest_buffered_ = slot.sequence;
}

} // namespace marketcapture
