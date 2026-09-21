#pragma once

#include "neripal/core/Autonomy.hpp"
#include "neripal/core/Care.hpp"
#include "neripal/core/GameEvent.hpp"
#include "neripal/core/IClock.hpp"
#include "neripal/core/IRandom.hpp"
#include "neripal/core/PetState.hpp"

#include <cstdint>

namespace neripal::core {

class Pet {
public:
    Pet(IClock& clock, IRandom& random);

    const PetState& state() const noexcept { return state_; }

    CareResult feed();
    CareResult train();
    CareResult sleep();
    CareResult wake();
    CareResult clean();
    CareResult apply(CareAction action);
    void update();
    void reset();

    // Consumes the oldest queued event. Returns false if empty and leaves `out`
    // unchanged. Overflow already dropped the oldest events (capacity
    // kGameEventCapacity); the queue never grows.
    bool pollEvent(GameEvent& out) noexcept;

    // Production-facing state restoration point for future persistence.
    // Values are normalized so corrupt saves cannot break core invariants.
    void restore(const PetState& state);

private:
    static int clampStat(int value) noexcept;
    void applyNeedsStep();
    void updateEvolution();
    void anchorClock();

    IClock& clock_;
    IRandom& random_;
    Autonomy autonomy_{};
    GameEventQueue events_{};
    PetState state_{};
    std::uint64_t lastUpdateMs_ = 0;
    std::uint64_t needsRemainderMs_ = 0;
};

}  // namespace neripal::core

