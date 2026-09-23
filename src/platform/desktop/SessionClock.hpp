#pragma once

#include "neripal/core/IClock.hpp"

#include <chrono>
#include <cstdint>

namespace neripal::desktop {

// Unscaled monotonic clock for autosave/debounce. Not used by Pet gameplay.
class SessionClock final : public core::IClock {
public:
    SessionClock() : start_(Clock::now()) {}

    std::uint64_t nowMillis() const override {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start_).count());
    }

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point start_;
};

}  // namespace neripal::desktop
