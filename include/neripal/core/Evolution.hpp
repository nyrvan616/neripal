#pragma once

#include <cstdint>

namespace neripal::core {

enum class EvolutionStage : std::uint8_t {
    Egg,
    Baby,
    Child,
    Adult,
};

namespace evolution {
// Kept as a preview-only duration until the full egg lifecycle is designed in 0.6.
inline constexpr std::uint64_t kEggHatchAgeMs = 60ULL * 60 * 1000;
inline constexpr std::uint64_t kChildAgeMs = 6ULL * 60 * 60 * 1000;
inline constexpr std::uint64_t kAdultAgeMs = 24ULL * 60 * 60 * 1000;
}

}  // namespace neripal::core
