#pragma once

#include "neripal/core/Care.hpp"
#include "neripal/core/PetState.hpp"

#include <cstdint>

namespace neripal::core {

enum class Need : std::uint8_t {
    Hunger,
    Energy,
    Hygiene,
    Affection,
    Stimulation,
};

enum class NeedLevel : std::uint8_t {
    Normal,
    Attention,
    Urgent,
};

// Provisional thresholds from Balance.hpp. Hunger is inverted against the other needs.
NeedLevel needLevel(Need need, int value) noexcept;

struct StatDelta {
    int hunger = 0;
    int energy = 0;
    int hygiene = 0;
    int affection = 0;
    int stimulation = 0;
    int happiness = 0;
    int health = 0;
};

// Provisional action effects. Sleep and Wake return an empty delta.
StatDelta careEffect(CareAction action) noexcept;

// One simulated minute. Advances `phase` through kNeedsPhaseCycle.
// Base-need decay, then happiness if any base need is Urgent, then health
// at the existing physical floors. Happiness and health are not recomputed
// from scratch.
void applyNeedsStep(PetState& state, std::uint8_t& phase) noexcept;

}  // namespace neripal::core
