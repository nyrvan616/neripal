#include "SystemWallClock.hpp"

#include <chrono>
#include <ctime>

namespace neripal::desktop {

std::optional<std::int64_t> SystemWallClock::nowUnixSeconds() const {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    if (localtime_s(&local, &time) != 0) {
        return std::nullopt;
    }
#else
    if (localtime_r(&time, &local) == nullptr) {
        return std::nullopt;
    }
#endif
    if (local.tm_year + 1900 < 2020) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(time);
}

}  // namespace neripal::desktop
