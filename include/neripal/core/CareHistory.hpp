#pragma once

#include "neripal/core/Evolution.hpp"
#include "neripal/core/Needs.hpp"
#include "neripal/core/PetState.hpp"

#include <cstddef>
#include <cstdint>

namespace neripal::core {

inline constexpr std::size_t kBaseNeedCount = 5;
// Indexed by EvolutionStage. Extra slots stay empty until a later stage is appended.
inline constexpr std::size_t kStageHistoryCapacity = 8;

enum class EpisodeState : std::uint8_t {
    None,
    Open,
    Counted,
};

enum class TrainingLevel : std::uint8_t {
    Low,
    Moderate,
    Frequent,
};

struct NeedEpisode {
    EpisodeState state = EpisodeState::None;
    std::uint8_t stepsRemaining = 0;
};

struct StageHistory {
    std::uint16_t careMistakes = 0;
    std::uint16_t trainCount = 0;
    std::uint32_t steps = 0;
    std::uint32_t healthGoodSteps = 0;
    std::uint32_t healthPoorSteps = 0;
    std::uint32_t happinessGoodSteps = 0;
    std::uint32_t happinessPoorSteps = 0;
    std::uint16_t responseCount = 0;
    std::uint32_t responseStepsSum = 0;
};

// Episodes are live attention windows. Stage slots are kept side by side so a
// later stage change can start a fresh slot without erasing the earlier ones.
struct CareRecord {
    NeedEpisode episodes[kBaseNeedCount]{};
    StageHistory stages[kStageHistoryCapacity]{};
    std::uint16_t lifetimeCareMistakes = 0;

    void clear() noexcept;
    void clearEpisodes() noexcept;
};

std::uint8_t stageHistoryIndex(EvolutionStage stage) noexcept;
StageHistory& historyFor(CareRecord& record, EvolutionStage stage) noexcept;
const StageHistory& historyFor(const CareRecord& record, EvolutionStage stage) noexcept;

TrainingLevel trainingLevel(const StageHistory& history) noexcept;

// One simulated minute of trend counters for the stage the pet is in now.
void recordStageStep(CareRecord& record, const PetState& state) noexcept;

// Awake minute only. Opens, decrements, expires, or closes episodes.
void tickCareEpisodes(CareRecord& record, const PetState& state) noexcept;

// Opens a window for each Urgent need that has none. Does not decrement.
void ensureUrgentEpisodes(CareRecord& record, const PetState& state) noexcept;

void noteTraining(CareRecord& record, EvolutionStage stage) noexcept;

}  // namespace neripal::core
