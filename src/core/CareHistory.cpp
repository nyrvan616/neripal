#include "neripal/core/CareHistory.hpp"

#include "neripal/core/Balance.hpp"

#include <limits>

namespace neripal::core {
namespace {

int needValue(const PetState& state, Need need) noexcept {
    switch (need) {
        case Need::Hunger: return state.hunger;
        case Need::Energy: return state.energy;
        case Need::Hygiene: return state.hygiene;
        case Need::Affection: return state.affection;
        case Need::Stimulation: return state.stimulation;
    }
    return 0;
}

void addMistake(CareRecord& record, StageHistory& history) noexcept {
    if (history.careMistakes < std::numeric_limits<std::uint16_t>::max()) {
        ++history.careMistakes;
    }
    if (record.lifetimeCareMistakes < std::numeric_limits<std::uint16_t>::max()) {
        ++record.lifetimeCareMistakes;
    }
}

void closeIfNeeded(NeedEpisode& episode, StageHistory& history) noexcept {
    if (episode.state == EpisodeState::Open) {
        const auto elapsed = static_cast<std::uint32_t>(balance::kAttentionWindowSteps -
                                                        episode.stepsRemaining);
        if (history.responseCount < std::numeric_limits<std::uint16_t>::max()) {
            ++history.responseCount;
        }
        history.responseStepsSum += elapsed;
    }
    episode = {};
}

}  // namespace

void CareRecord::clear() noexcept {
    *this = {};
}

void CareRecord::clearEpisodes() noexcept {
    for (auto& episode : episodes) {
        episode = {};
    }
}

std::uint8_t stageHistoryIndex(EvolutionStage stage) noexcept {
    const auto index = static_cast<std::uint8_t>(stage);
    if (index >= kStageHistoryCapacity) {
        return static_cast<std::uint8_t>(kStageHistoryCapacity - 1);
    }
    return index;
}

StageHistory& historyFor(CareRecord& record, EvolutionStage stage) noexcept {
    return record.stages[stageHistoryIndex(stage)];
}

const StageHistory& historyFor(const CareRecord& record, EvolutionStage stage) noexcept {
    return record.stages[stageHistoryIndex(stage)];
}

TrainingLevel trainingLevel(const StageHistory& history) noexcept {
    if (history.trainCount >= balance::kTrainingFrequentCount) {
        return TrainingLevel::Frequent;
    }
    if (history.trainCount >= balance::kTrainingModerateCount) {
        return TrainingLevel::Moderate;
    }
    return TrainingLevel::Low;
}

void recordStageSteps(CareRecord& record, const PetState& state, std::uint32_t count) noexcept {
    if (count == 0) {
        return;
    }
    auto& history = historyFor(record, state.stage);
    history.steps += count;
    if (state.health >= balance::kHistoryGoodStat) {
        history.healthGoodSteps += count;
    } else if (state.health < balance::kHistoryPoorStat) {
        history.healthPoorSteps += count;
    }
    if (state.happiness >= balance::kHistoryGoodStat) {
        history.happinessGoodSteps += count;
    } else if (state.happiness < balance::kHistoryPoorStat) {
        history.happinessPoorSteps += count;
    }
}

void recordStageStep(CareRecord& record, const PetState& state) noexcept {
    recordStageSteps(record, state, 1);
}

void ensureUrgentEpisodes(CareRecord& record, const PetState& state) noexcept {
    if (state.sleeping) {
        return;
    }
    for (std::size_t index = 0; index < kBaseNeedCount; ++index) {
        const auto need = static_cast<Need>(index);
        if (needLevel(need, needValue(state, need)) != NeedLevel::Urgent) {
            continue;
        }
        auto& episode = record.episodes[index];
        if (episode.state != EpisodeState::None) {
            continue;
        }
        episode.state = EpisodeState::Open;
        episode.stepsRemaining = balance::kAttentionWindowSteps;
    }
}

void tickCareEpisodes(CareRecord& record, const PetState& state) noexcept {
    if (state.sleeping) {
        return;
    }
    auto& history = historyFor(record, state.stage);
    for (std::size_t index = 0; index < kBaseNeedCount; ++index) {
        const auto need = static_cast<Need>(index);
        auto& episode = record.episodes[index];
        const bool urgent = needLevel(need, needValue(state, need)) == NeedLevel::Urgent;
        if (!urgent) {
            if (episode.state != EpisodeState::None) {
                closeIfNeeded(episode, history);
            }
            continue;
        }
        if (episode.state == EpisodeState::None) {
            episode.state = EpisodeState::Open;
            episode.stepsRemaining = balance::kAttentionWindowSteps;
            continue;
        }
        if (episode.state != EpisodeState::Open) {
            continue;
        }
        if (episode.stepsRemaining > 0) {
            --episode.stepsRemaining;
        }
        if (episode.stepsRemaining == 0) {
            episode.state = EpisodeState::Counted;
            addMistake(record, history);
        }
    }
}

void noteTraining(CareRecord& record, EvolutionStage stage) noexcept {
    auto& history = historyFor(record, stage);
    if (history.trainCount < std::numeric_limits<std::uint16_t>::max()) {
        ++history.trainCount;
    }
}

}  // namespace neripal::core
