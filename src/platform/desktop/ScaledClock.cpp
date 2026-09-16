#include "ScaledClock.hpp"

#include <algorithm>

namespace neripal::desktop {

ScaledClock::ScaledClock() : realAnchor_(SteadyClock::now()) {}

std::uint64_t ScaledClock::nowMillis() const {
    const auto realElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        SteadyClock::now() - realAnchor_).count();
    return simulatedAnchorMs_ + static_cast<std::uint64_t>(realElapsed) * scale_;
}

void ScaledClock::setScale(int scale) {
    scale = std::clamp(scale, 1, 1000);
    simulatedAnchorMs_ = nowMillis();
    realAnchor_ = SteadyClock::now();
    scale_ = scale;
}

void ScaledClock::advance(std::uint64_t milliseconds) {
    simulatedAnchorMs_ = nowMillis() + milliseconds;
    realAnchor_ = SteadyClock::now();
}

}  // namespace neripal::desktop
