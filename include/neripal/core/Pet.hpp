#pragma once

#include "neripal/core/IClock.hpp"
#include "neripal/core/PetState.hpp"

#include <cstdint>

namespace neripal::core {

class Pet {
public:
    explicit Pet(IClock& clock);

    const PetState& state() const noexcept { return state_; }

    void feed();
    void train();
    void sleep();
    void wake();
    void update();
    void reset();

    // Production-facing state restoration point for future persistence.
    // Values are normalized so corrupt saves cannot break core invariants.
    void restore(const PetState& state);

private:
    static int clampStat(int value) noexcept;
    void applyNeedsStep();
    void updateEvolution();
    void anchorClock();

    IClock& clock_;
    PetState state_{};
    std::uint64_t lastUpdateMs_ = 0;
    std::uint64_t needsRemainderMs_ = 0;
};

}  // namespace neripal::core
