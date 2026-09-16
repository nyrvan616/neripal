#pragma once

#include <cstdint>

namespace neripal::core {

enum class EvolutionStage : std::uint8_t {
    Baby,
    Child,
    Adult,
};

namespace evolution {
inline constexpr std::uint64_t kChildAgeMs = 6ULL * 60 * 60 * 1000;
inline constexpr std::uint64_t kAdultAgeMs = 24ULL * 60 * 60 * 1000;
}

}  // namespace neripal::core
