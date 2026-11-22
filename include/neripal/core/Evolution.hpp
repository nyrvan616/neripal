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

// Visual variant, separate from the life stage. AdultA/B/C/Secret are
// validation fixtures, not approved creature identities and not a ranking.
enum class FormId : std::uint8_t {
    None,
    Juvenile,
    AdultA,
    AdultB,
    AdultC,
    AdultSecret,
};

namespace evolution {
// Provisional durations from the moment Pet::hatch() sets age back to 0.
// Age opens Baby, Child, Adult and Final. It does not open the egg or choose a form.
// kEggHatchAgeMs is the former one-hour wait and is not a stage gate.
inline constexpr std::uint64_t kEggHatchAgeMs = 60ULL * 60 * 1000;
inline constexpr std::uint64_t kChildAgeMs = 6ULL * 60 * 60 * 1000;
inline constexpr std::uint64_t kAdultAgeMs = 24ULL * 60 * 60 * 1000;
inline constexpr std::uint64_t kFinalAgeMs = 72ULL * 60 * 60 * 1000;
}

}  // namespace neripal::core
