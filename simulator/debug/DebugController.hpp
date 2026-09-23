#pragma once

#include "neripal/core/Pet.hpp"
#include "neripal/persist/SaveSession.hpp"
#include "platform/desktop/ScaledClock.hpp"

namespace neripal::simulator {

class DebugController {
public:
    DebugController(core::Pet& pet, desktop::ScaledClock& clock, persist::SaveSession& saves)
        : pet_(pet), clock_(clock), saves_(saves) {}

    void setTimeScale(int scale) { clock_.setScale(scale); }
    void advanceMinutes(std::uint64_t minutes) {
        clock_.advance(minutes * 60'000ULL);
        saves_.markDirty();
    }
    int timeScale() const noexcept { return clock_.scale(); }
    void setHunger(int value);
    void setHappiness(int value);
    void setEnergy(int value);
    void setHealth(int value);
    void setHygiene(int value);
    void adjustStat(int index, int delta);
    void feed() { saves_.noteCareResult(pet_.feed()); }
    void train() { saves_.noteCareResult(pet_.train()); }
    void sleep() { saves_.noteCareResult(pet_.sleep()); }
    void wake() { saves_.noteCareResult(pet_.wake()); }
    void clean() { saves_.noteCareResult(pet_.clean()); }
    void reset();
    void forceEvolution();

private:
    void restore(core::PetState state);
    core::Pet& pet_;
    desktop::ScaledClock& clock_;
    persist::SaveSession& saves_;
};

}  // namespace neripal::simulator
