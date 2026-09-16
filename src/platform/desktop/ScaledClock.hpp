#pragma once

#include "neripal/core/IClock.hpp"

#include <chrono>
#include <cstdint>

namespace neripal::desktop {

class ScaledClock final : public core::IClock {
public:
    ScaledClock();

    std::uint64_t nowMillis() const override;
    void setScale(int scale);
    void advance(std::uint64_t milliseconds);
    int scale() const noexcept { return scale_; }

private:
    using SteadyClock = std::chrono::steady_clock;
    SteadyClock::time_point realAnchor_;
    std::uint64_t simulatedAnchorMs_ = 0;
    int scale_ = 1;
};

}  // namespace neripal::desktop
