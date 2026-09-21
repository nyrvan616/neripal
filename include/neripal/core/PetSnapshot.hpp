#pragma once

#include "neripal/core/Evolution.hpp"
#include "neripal/core/SleepCause.hpp"

#include <cstdint>

namespace neripal::core {

// Domain state that survives save/load. Presentation pose is reconstructed.
struct PetSnapshot {
    int hunger = 35;
    int happiness = 75;
    int energy = 80;
    int health = 100;
    int hygiene = 80;
    std::uint64_t ageMillis = 0;
    std::uint32_t needsRemainderMs = 0;
    EvolutionStage stage = EvolutionStage::Baby;
    SleepCause sleepCause = SleepCause::None;
    std::uint32_t napRemainingMs = 0;
};

// If energy reaches kNapWakeEnergy before napRemainingMs elapses, Nap ends and
// the leftover offline interval uses awake need rules. applyOffline does this
// without starting another nap.

}  // namespace neripal::core
