#pragma once

#include <cstdint>

namespace neripal::core {

enum class EvolutionStage : std::uint8_t {
    Egg,
    Baby,
    Child,
    Adult,
    Final,
};

namespace evolution {
// Provisional 0.6 durations. Age opens the next stage; it does not choose a form.
inline constexpr std::uint64_t kEggHatchAgeMs = 60ULL * 60 * 1000;
inline constexpr std::uint64_t kChildAgeMs = 6ULL * 60 * 60 * 1000;
inline constexpr std::uint64_t kAdultAgeMs = 24ULL * 60 * 60 * 1000;
inline constexpr std::uint64_t kFinalAgeMs = 72ULL * 60 * 60 * 1000;
}

}  // namespace neripal::core
