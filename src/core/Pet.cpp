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

void Pet::feed() {
    state_.hunger = clampStat(state_.hunger + balance::kFeedHunger);
    state_.happiness = clampStat(state_.happiness + balance::kFeedHappiness);
}

void Pet::train() {
    state_.energy = clampStat(state_.energy + balance::kTrainEnergy);
    state_.hunger = clampStat(state_.hunger + balance::kTrainHunger);
    state_.happiness = clampStat(state_.happiness + balance::kTrainHappiness);
    state_.health = clampStat(state_.health + balance::kTrainHealth);
}

void Pet::sleep() {
    state_.sleeping = true;
}

void Pet::wake() {
    state_.sleeping = false;
}

void Pet::applyNeedsStep() {
    state_.hunger = clampStat(state_.hunger + balance::kHungerPerStep);
    state_.energy = clampStat(state_.energy +
        (state_.sleeping ? balance::kSleepEnergyPerStep : balance::kAwakeEnergyPerStep));

    if (state_.hunger >= balance::kNeglectThreshold) {
        state_.happiness = clampStat(state_.happiness + balance::kHappinessNeglectPerStep);
    }
    if (state_.hunger == balance::kMaxStat || state_.energy == balance::kMinStat) {
        state_.health = clampStat(state_.health + balance::kCriticalHealthPerStep);
    }
}

void Pet::updateEvolution() {
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
    updateEvolution();
    needsRemainderMs_ = 0;
    anchorClock();
}

}  // namespace neripal::core
