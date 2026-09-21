#pragma once

#include "neripal/core/Balance.hpp"
#include "neripal/core/PetState.hpp"

#include <cstdint>

namespace neripal::core {

enum class Mood : std::uint8_t {
    Calm,
    Happy,
    Annoyed,
    Tired,
    Dirty,
    Resting,
};

// Derived from the current snapshot. Not stored on PetState.
// Egg is not a mood: presentation shows WAITING and does not use care mood.
// Priority is first match: Resting, Tired, Dirty, Annoyed, Happy, Calm.
inline Mood deriveMood(const PetState& state) noexcept {
    if (state.sleeping) {
        return Mood::Resting;
    }
    if (state.energy <= balance::kTiredMoodEnergy) {
        return Mood::Tired;
    }
    if (state.hygiene <= balance::kHygieneNeglectThreshold) {
        return Mood::Dirty;
    }
    if (state.happiness <= balance::kAnnoyedMoodHappiness ||
        state.hunger >= balance::kAnnoyedMoodHunger ||
        state.health < balance::kAnnoyedMoodHealth) {
        return Mood::Annoyed;
    }
    if (state.happiness > balance::kHappyMoodHappiness) {
        return Mood::Happy;
    }
    return Mood::Calm;
}

}  // namespace neripal::core
