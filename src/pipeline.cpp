#include "marketcapture/pipeline.hpp"
#include <stdexcept>
#include <utility>

namespace marketcapture {

CapturePipeline::CapturePipeline(EventHandler handler) : handler_(std::move(handler)) {
    if (!handler_) throw std::invalid_argument("pipeline event handler is required");
}

std::size_t CapturePipeline::process(std::span<const std::uint8_t> datagram) {
    metrics_.packet_received();
    MoldPacket packet;
    try {
        packet = mold_.decode(datagram);
    } catch (...) {
        metrics_.parse_error();
        throw;
    }

    if (next_sequence_) {
        if (packet.sequence < *next_sequence_)
            throw ParseError("stale or out-of-order MoldUDP64 sequence");
        if (packet.sequence > *next_sequence_)
            metrics_.gap(packet.sequence - *next_sequence_);
    }

    std::size_t processed = 0;
    for (std::size_t i = 0; i < packet.messages.size(); ++i) {
        try {
            auto event = itch_.parse(packet.messages[i]);
            handler_(packet.sequence + i, event);
            metrics_.message_processed();
            ++processed;
        } catch (...) {
            metrics_.parse_error();
            throw;
        }
    }
    next_sequence_ = packet.sequence + packet.messages.size();
    return processed;
}

void CapturePipeline::reset() noexcept {
    next_sequence_.reset();
}

} // namespace marketcapture
