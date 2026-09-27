#pragma once

#include <cstdint>

namespace neripal::core {

enum class SleepCause : std::uint8_t {
    None = 0,
    Player = 1,
    Nap = 2,
};

}  // namespace neripal::core
