#pragma once

#include "neripal/core/Activity.hpp"
#include "neripal/core/Evolution.hpp"

#include <cstdint>

namespace neripal::core {

struct PetState {
    int hunger = 35;       // 0 = full, 100 = maximum hunger
    int happiness = 75;    // 0 = minimum, 100 = maximum
    int energy = 80;       // 0 = empty, 100 = full
    int health = 100;      // 0 = minimum, 100 = maximum
    int hygiene = 80;      // 0 = dirty, 100 = clean
    std::uint64_t ageMillis = 0;
    bool sleeping = false;
    EvolutionStage stage = EvolutionStage::Baby;

    // Presentation snapshot of the current activity. Mood is not stored here.
    Activity activity = Activity::Idle;
    IdleVariant idleVariant = IdleVariant::Bob;
    int facing = 1;  // -1 left, +1 right
    int x = 120;     // logical 0–239; reset/restore snap to home center
    std::uint32_t activityElapsedMs = 0;
    std::uint32_t activityDurationMs = 0;  // 0 = until interrupt (Sleep)
};

}  // namespace neripal::core

