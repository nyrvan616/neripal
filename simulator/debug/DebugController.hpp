#pragma once

#include "neripal/core/Pet.hpp"
#include "platform/desktop/ScaledClock.hpp"

namespace neripal::simulator {

class DebugController {
public:
    DebugController(core::Pet& pet, desktop::ScaledClock& clock) : pet_(pet), clock_(clock) {}

    void setTimeScale(int scale) { clock_.setScale(scale); }
    void advanceMinutes(std::uint64_t minutes) { clock_.advance(minutes * 60'000ULL); }
    int timeScale() const noexcept { return clock_.scale(); }
    void setHunger(int value);
    void setHappiness(int value);
    void setEnergy(int value);
    void setHealth(int value);
    void adjustStat(int index, int delta);
    void feed() { pet_.feed(); }
    void train() { pet_.train(); }
    void sleep() { pet_.sleep(); }
    void wake() { pet_.wake(); }
    void reset() { pet_.reset(); }

private:
    void restore(core::PetState state);
    core::Pet& pet_;
    desktop::ScaledClock& clock_;
};

}  // namespace neripal::simulator
