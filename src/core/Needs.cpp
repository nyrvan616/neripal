#include "neripal/core/Needs.hpp"

#include "neripal/core/Balance.hpp"

#include <algorithm>

namespace neripal::core {
namespace {

int clampStat(int value) noexcept {
    return std::clamp(value, balance::kMinStat, balance::kMaxStat);
}

bool phaseHit(std::uint8_t phase, int interval) noexcept {
    return interval > 0 && (phase % static_cast<std::uint8_t>(interval)) == 0;
}

}  // namespace

NeedLevel needLevel(Need need, int value) noexcept {
    if (need == Need::Hunger) {
        if (value >= balance::kHungerUrgent) return NeedLevel::Urgent;
        if (value >= balance::kHungerAttention) return NeedLevel::Attention;
        return NeedLevel::Normal;
    }
    if (value <= balance::kLowNeedUrgent) return NeedLevel::Urgent;
    if (value <= balance::kLowNeedAttention) return NeedLevel::Attention;
    return NeedLevel::Normal;
}

StatDelta careEffect(CareAction action) noexcept {
    switch (action) {
        case CareAction::Feed:
            return StatDelta{balance::kFeedHunger, 0, balance::kFeedHygiene, 0, 0,
                             balance::kFeedHappiness, 0};
        case CareAction::Train:
            return StatDelta{balance::kTrainHunger, balance::kTrainEnergy, balance::kTrainHygiene, 0,
                             balance::kTrainStimulation, balance::kTrainHappiness,
                             balance::kTrainHealth};
        case CareAction::Clean:
            return StatDelta{0, 0, balance::kCleanHygiene, 0, 0, balance::kCleanHappiness, 0};
        case CareAction::Pet:
            return StatDelta{0, 0, 0, balance::kPetAffection, 0, balance::kPetHappiness, 0};
        case CareAction::Play:
            return StatDelta{balance::kPlayHunger, balance::kPlayEnergy, 0, 0,
                             balance::kPlayStimulation, balance::kPlayHappiness, 0};
        case CareAction::Sleep:
        case CareAction::Wake:
            break;
    }
    return {};
}

void applyNeedsStep(PetState& state, std::uint8_t& phase) noexcept {
    phase = static_cast<std::uint8_t>((phase + 1u) % balance::kNeedsPhaseCycle);

    state.hunger = clampStat(state.hunger + balance::kHungerPerStep);
    state.energy = clampStat(state.energy + (state.sleeping ? balance::kSleepEnergyPerStep
                                                            : balance::kAwakeEnergyPerStep));
    if (state.sleeping) {
        if (phaseHit(phase, balance::kHygieneSleepInterval)) {
            state.hygiene = clampStat(state.hygiene + balance::kHygienePerAwakeStep);
        }
        if (phaseHit(phase, balance::kAffectionSleepInterval)) {
            state.affection = clampStat(state.affection + balance::kAffectionPerHit);
        }
    } else {
        state.hygiene = clampStat(state.hygiene + balance::kHygienePerAwakeStep);
        if (phaseHit(phase, balance::kAffectionAwakeInterval)) {
            state.affection = clampStat(state.affection + balance::kAffectionPerHit);
        }
        state.stimulation = clampStat(state.stimulation + balance::kStimulationAwakePerStep);
    }

    const bool urgent = needLevel(Need::Hunger, state.hunger) == NeedLevel::Urgent ||
                        needLevel(Need::Energy, state.energy) == NeedLevel::Urgent ||
                        needLevel(Need::Hygiene, state.hygiene) == NeedLevel::Urgent ||
                        needLevel(Need::Affection, state.affection) == NeedLevel::Urgent ||
                        needLevel(Need::Stimulation, state.stimulation) == NeedLevel::Urgent;
    if (urgent) {
        state.happiness = clampStat(state.happiness + balance::kHappinessUrgentPerStep);
    }
    if (state.hunger == balance::kMaxStat || state.energy == balance::kMinStat ||
        state.hygiene == balance::kMinStat) {
        state.health = clampStat(state.health + balance::kCriticalHealthPerStep);
    }
}

}  // namespace neripal::core
