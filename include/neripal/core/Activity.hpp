#pragma once

#include <cstdint>

namespace neripal::core {

enum class Activity : std::uint8_t {
    Idle,
    Walk,
    Eat,
    Happy,
    Annoyed,
    Tired,
    Dirty,
    Sleep,
    Nap,
};

enum class IdleVariant : std::uint8_t {
    Bob,
    Glance,
    Bounce,
    Fidget,
    Slump,
    Shake,
};

}  // namespace neripal::core
