#include "marketcapture/exchange_simulator.hpp"
#include <algorithm>
#include <stdexcept>

namespace marketcapture {
namespace {
void put(std::vector<std::uint8_t>& value, std::size_t position,
         std::uint64_t number, std::size_t bytes) {
    for (std::size_t i = 0; i < bytes; ++i)
        value[position + bytes - 1 - i] = static_cast<std::uint8_t>(number >> (i * 8));
}
}

ExchangeSimulator::ExchangeSimulator(std::uint64_t seed, std::vector<std::string> symbols)
    : state_(seed ? seed : 1), symbols_(std::move(symbols)) {
    if (symbols_.empty()) throw std::invalid_argument("simulator needs at least one symbol");
    for (const auto& symbol : symbols_)
        if (symbol.empty() || symbol.size() > 8)
            throw std::invalid_argument("simulator symbol must contain 1-8 characters");
}

std::uint64_t ExchangeSimulator::random() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 7;
    state_ ^= state_ << 17;
    return state_;
}

std::vector<std::uint8_t> ExchangeSimulator::add_message(std::uint64_t id,
                                                         const std::string& symbol) {
    std::vector<std::uint8_t> message(36, ' ');
    message[0] = 'A';
    put(message, 5, sequence_ * 100, 6);
    put(message, 11, id, 8);
    message[19] = random() % 2 ? 'B' : 'S';
    put(message, 20, 1 + random() % 1000, 4);
    std::copy(symbol.begin(), symbol.end(), message.begin() + 24);
    put(message, 32, 100000 + random() % 5000000, 4);
    return message;
}

std::vector<std::uint8_t> ExchangeSimulator::delete_message(std::uint64_t id) {
    std::vector<std::uint8_t> message(19);
    message[0] = 'D';
    put(message, 5, sequence_ * 100, 6);
    put(message, 11, id, 8);
    return message;
}

std::vector<std::uint8_t> ExchangeSimulator::mold(
    std::uint64_t sequence, const std::vector<std::uint8_t>& message) const {
    std::vector<std::uint8_t> packet(22 + message.size());
    const char session[10] = {'S','I','M','0','0','0','0','0','0','1'};
    std::copy(std::begin(session), std::end(session), packet.begin());
    put(packet, 10, sequence, 8); put(packet, 18, 1, 2); put(packet, 20, message.size(), 2);
    std::copy(message.begin(), message.end(), packet.begin() + 22);
    return packet;
}

SimulatedPacket ExchangeSimulator::next() {
    std::vector<std::uint8_t> message;
    if (live_orders_.empty() || random() % 3 != 0) {
        const auto id = next_order_id_++;
        live_orders_.push_back(id);
        message = add_message(id, symbols_[random() % symbols_.size()]);
    } else {
        const auto index = static_cast<std::size_t>(random() % live_orders_.size());
        const auto id = live_orders_[index];
        message = delete_message(id);
        live_orders_.erase(live_orders_.begin() + static_cast<std::ptrdiff_t>(index));
    }
    SimulatedPacket result{sequence_, mold(sequence_, message)};
    ++sequence_;
    return result;
}

std::vector<SimulatedPacket> ExchangeSimulator::generate(std::size_t count) {
    std::vector<SimulatedPacket> result;
    result.reserve(count);
    while (result.size() < count) result.push_back(next());
    return result;
}

} // namespace marketcapture
