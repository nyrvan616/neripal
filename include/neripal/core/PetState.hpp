#pragma once

#include "neripal/core/Evolution.hpp"

#include <cstdint>

namespace neripal::core {

struct PetState {
    int hunger = 35;       // 0 = full, 100 = maximum hunger
    int happiness = 75;    // 0 = minimum, 100 = maximum
    int energy = 80;       // 0 = empty, 100 = full
    int health = 100;      // 0 = minimum, 100 = maximum
    std::uint64_t ageMillis = 0;
    bool sleeping = false;
    EvolutionStage stage = EvolutionStage::Baby;
};

}  // namespace neripal::core
