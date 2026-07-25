#pragma once

#include "marketcapture/types.hpp"
#include <filesystem>
#include <fstream>
#include <functional>

namespace marketcapture {

class TickRecorder {
public:
    explicit TickRecorder(const std::filesystem::path& path);
    void write(const Event& event);
    void flush();
private:
    std::ofstream output_;
};

class ReplayEngine {
public:
    using Handler = std::function<void(const Event&)>;
    std::size_t replay(const std::filesystem::path& path, const Handler& handler) const;
};

} // namespace marketcapture
