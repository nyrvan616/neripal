#include "Esp32WallClock.hpp"

#include <ctime>
#include <time.h>

namespace neripal::waveshare_s3 {

std::optional<std::int64_t> Esp32WallClock::nowUnixSeconds() const {
    const std::time_t now = time(NULL);
    if (now == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }
    std::tm local{};
    if (localtime_r(&now, &local) == nullptr) {
        return std::nullopt;
    }
    if (local.tm_year + 1900 < 2020) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(now);
}

}  // namespace neripal::waveshare_s3
