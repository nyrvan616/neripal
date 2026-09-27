#pragma once

#include "neripal/core/Balance.hpp"
#include "neripal/core/CareHistory.hpp"
#include "neripal/core/EvolutionNotice.hpp"
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
    int affection = balance::kAffectionStart;
    int stimulation = balance::kStimulationStart;
    std::uint64_t ageMillis = 0;
    std::uint32_t needsRemainderMs = 0;
    std::uint8_t needsStepPhase = 0;
    EvolutionStage stage = EvolutionStage::Egg;
    FormId form = FormId::None;
    SleepCause sleepCause = SleepCause::None;
    std::uint32_t napRemainingMs = 0;
    CareRecord care{};
    EvolutionNotice notices[kEvolutionNoticeCapacity]{};
    std::uint8_t noticeCount = 0;
};

// If energy reaches kNapWakeEnergy before napRemainingMs elapses, Nap ends and
// the leftover offline interval uses awake need rules. applyOffline does this
// without starting another nap.

}  // namespace neripal::core
