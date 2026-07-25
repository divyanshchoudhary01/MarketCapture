#include "marketcapture/config.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace marketcapture {
namespace {
std::string trim(std::string value) {
    const auto whitespace = [](unsigned char c) { return std::isspace(c); };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), whitespace));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), whitespace).base(), value.end());
    return value;
}

std::uint64_t number(std::string_view value, std::string_view name) {
    std::size_t consumed = 0;
    std::uint64_t result{};
    try {
        result = std::stoull(std::string(value), &consumed);
    } catch (...) {
        throw std::invalid_argument("invalid numeric config value for " + std::string(name));
    }
    if (consumed != value.size())
        throw std::invalid_argument("invalid numeric config value for " + std::string(name));
    return result;
}
}

LiveFeedConfig LiveFeedConfig::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open live feed config: " + path.string());
    LiveFeedConfig config;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto separator = line.find('=');
        if (separator == std::string::npos)
            throw std::invalid_argument("invalid config line " + std::to_string(line_number));
        const auto key = trim(line.substr(0, separator));
        const auto value = trim(line.substr(separator + 1));
        if (key == "multicast_group") config.multicast_group = value;
        else if (key == "port") {
            const auto parsed = number(value, key);
            if (parsed > std::numeric_limits<std::uint16_t>::max())
                throw std::invalid_argument("port is out of range");
            config.port = static_cast<std::uint16_t>(parsed);
        } else if (key == "interface_address") config.interface_address = value;
        else if (key == "receive_buffer_bytes") {
            const auto parsed = number(value, key);
            if (parsed > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
                throw std::invalid_argument("receive_buffer_bytes is out of range");
            config.receive_buffer_bytes = static_cast<int>(parsed);
        } else if (key == "record_path") config.record_path = value;
        else if (key == "pcap_path") config.pcap_path = value;
        else if (key == "max_packets") config.max_packets = number(value, key);
        else throw std::invalid_argument("unknown config key: " + key);
    }
    config.validate();
    return config;
}

void LiveFeedConfig::validate() const {
    if (multicast_group.empty()) throw std::invalid_argument("multicast_group is required");
    if (port == 0) throw std::invalid_argument("port must be between 1 and 65535");
    if (interface_address.empty()) throw std::invalid_argument("interface_address is required");
    if (receive_buffer_bytes < 64 * 1024)
        throw std::invalid_argument("receive_buffer_bytes must be at least 65536");
}
} // namespace marketcapture
