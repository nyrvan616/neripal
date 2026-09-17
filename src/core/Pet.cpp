#include "neripal/core/Pet.hpp"

#include "neripal/core/Balance.hpp"

#include <algorithm>

namespace neripal::core {

Pet::Pet(IClock& clock) : clock_(clock) {
    anchorClock();
}

int Pet::clampStat(int value) noexcept {
    return std::clamp(value, balance::kMinStat, balance::kMaxStat);
}

void Pet::anchorClock() {
    lastUpdateMs_ = clock_.nowMillis();
}

CareResult Pet::feed() {
    if (state_.sleeping) return CareResult::RejectedAsleep;
    state_.hunger = clampStat(state_.hunger + balance::kFeedHunger);
    state_.happiness = clampStat(state_.happiness + balance::kFeedHappiness);
    state_.hygiene = clampStat(state_.hygiene + balance::kFeedHygiene);
    return CareResult::Applied;
}

CareResult Pet::train() {
    if (state_.sleeping) return CareResult::RejectedAsleep;
    if (state_.energy < -balance::kTrainEnergy) return CareResult::RejectedNoEnergy;
    state_.energy = clampStat(state_.energy + balance::kTrainEnergy);
    state_.hunger = clampStat(state_.hunger + balance::kTrainHunger);
    state_.happiness = clampStat(state_.happiness + balance::kTrainHappiness);
    state_.health = clampStat(state_.health + balance::kTrainHealth);
    state_.hygiene = clampStat(state_.hygiene + balance::kTrainHygiene);
    return CareResult::Applied;
}

CareResult Pet::sleep() {
    if (state_.sleeping) return CareResult::RejectedAlreadySleeping;
    state_.sleeping = true;
    return CareResult::Applied;
}

CareResult Pet::wake() {
    if (!state_.sleeping) return CareResult::RejectedAlreadyAwake;
    state_.sleeping = false;
    return CareResult::Applied;
}

CareResult Pet::clean() {
    if (state_.sleeping) return CareResult::RejectedAsleep;
    state_.hygiene = clampStat(state_.hygiene + balance::kCleanHygiene);
    state_.happiness = clampStat(state_.happiness + balance::kCleanHappiness);
    return CareResult::Applied;
}

void Pet::applyNeedsStep() {
    state_.hunger = clampStat(state_.hunger + balance::kHungerPerStep);
    state_.energy = clampStat(state_.energy +
        (state_.sleeping ? balance::kSleepEnergyPerStep : balance::kAwakeEnergyPerStep));
    if (!state_.sleeping) {
        state_.hygiene = clampStat(state_.hygiene + balance::kHygienePerAwakeStep);
    }

    if (state_.hunger >= balance::kNeglectThreshold ||
        state_.hygiene <= balance::kHygieneNeglectThreshold) {
        state_.happiness = clampStat(state_.happiness + balance::kHappinessNeglectPerStep);
    }
    if (state_.hunger == balance::kMaxStat || state_.energy == balance::kMinStat ||
        state_.hygiene == balance::kMinStat) {
        state_.health = clampStat(state_.health + balance::kCriticalHealthPerStep);
    }
}

void Pet::updateEvolution() {
    if (state_.stage == EvolutionStage::Egg) {
        if (state_.ageMillis < evolution::kEggHatchAgeMs) return;
        state_.stage = EvolutionStage::Baby;
    }
    if (state_.ageMillis >= evolution::kAdultAgeMs) {
        state_.stage = EvolutionStage::Adult;
    } else if (state_.ageMillis >= evolution::kChildAgeMs) {
        state_.stage = EvolutionStage::Child;
    } else {
        state_.stage = EvolutionStage::Baby;
    }
}

void Pet::update() {
    const auto now = clock_.nowMillis();
    if (now < lastUpdateMs_) {
        lastUpdateMs_ = now;
        return;
    }

    const auto elapsed = now - lastUpdateMs_;
    lastUpdateMs_ = now;
    state_.ageMillis += elapsed;
    updateEvolution();
    needsRemainderMs_ += elapsed;

    while (needsRemainderMs_ >= balance::kNeedsStepMs) {
        needsRemainderMs_ -= balance::kNeedsStepMs;
        applyNeedsStep();
    }
}

void Pet::reset() {
    state_ = PetState{};
    needsRemainderMs_ = 0;
    anchorClock();
}

void Pet::restore(const PetState& state) {
    state_ = state;
    state_.hunger = clampStat(state_.hunger);
    state_.happiness = clampStat(state_.happiness);
    state_.energy = clampStat(state_.energy);
    state_.health = clampStat(state_.health);
    state_.hygiene = clampStat(state_.hygiene);
    updateEvolution();
    needsRemainderMs_ = 0;
    anchorClock();
}

}  // namespace neripal::core
