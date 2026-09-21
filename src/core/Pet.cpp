#include "neripal/core/Pet.hpp"

#include "neripal/core/Balance.hpp"
#include "neripal/core/Care.hpp"

#include <algorithm>

namespace neripal::core {

Pet::Pet(IClock& clock, IRandom& random) : clock_(clock), random_(random) {
    autonomy_.reset(state_);
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
    autonomy_.onCareApplied(CareAction::Feed, state_, events_);
    return CareResult::Applied;
}

CareResult Pet::train() {
    if (state_.sleeping) return CareResult::RejectedAsleep;
    if (state_.energy < -balance::kTrainEnergy) {
        autonomy_.onRejectedNoEnergy(state_, events_);
        return CareResult::RejectedNoEnergy;
    }
    state_.energy = clampStat(state_.energy + balance::kTrainEnergy);
    state_.hunger = clampStat(state_.hunger + balance::kTrainHunger);
    state_.happiness = clampStat(state_.happiness + balance::kTrainHappiness);
    state_.health = clampStat(state_.health + balance::kTrainHealth);
    state_.hygiene = clampStat(state_.hygiene + balance::kTrainHygiene);
    autonomy_.onCareApplied(CareAction::Train, state_, events_);
    return CareResult::Applied;
}

CareResult Pet::sleep() {
    if (state_.sleeping) return CareResult::RejectedAlreadySleeping;
    state_.sleeping = true;
    autonomy_.enterSleep(state_, events_);
    return CareResult::Applied;
}

CareResult Pet::wake() {
    if (!state_.sleeping) return CareResult::RejectedAlreadyAwake;
    state_.sleeping = false;
    autonomy_.enterIdle(state_, events_);
    return CareResult::Applied;
}

CareResult Pet::clean() {
    if (state_.sleeping) return CareResult::RejectedAsleep;
    state_.hygiene = clampStat(state_.hygiene + balance::kCleanHygiene);
    state_.happiness = clampStat(state_.happiness + balance::kCleanHappiness);
    autonomy_.onCareApplied(CareAction::Clean, state_, events_);
    return CareResult::Applied;
}

CareResult Pet::apply(CareAction action) {
    switch (action) {
        case CareAction::Feed: return feed();
        case CareAction::Train: return train();
        case CareAction::Sleep: return sleep();
        case CareAction::Wake: return wake();
        case CareAction::Clean: return clean();
    }
    return CareResult::RejectedAlreadyAwake;
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
    const bool startedAsEgg = state_.stage == EvolutionStage::Egg;
    state_.ageMillis += elapsed;
    updateEvolution();
    needsRemainderMs_ += elapsed;

    while (needsRemainderMs_ >= balance::kNeedsStepMs) {
        needsRemainderMs_ -= balance::kNeedsStepMs;
        applyNeedsStep();
    }

    autonomy_.onStatsChanged(state_, events_);
    const bool frozen = startedAsEgg || autonomy_.frozenBySleep(state_);
    autonomy_.advance(state_, random_, events_, elapsed, frozen);
}

bool Pet::pollEvent(GameEvent& out) noexcept {
    return events_.poll(out);
}

void Pet::reset() {
    state_ = PetState{};
    needsRemainderMs_ = 0;
    events_.clear();
    autonomy_.reset(state_);
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
    events_.clear();
    autonomy_.reset(state_);
    anchorClock();
}

}  // namespace neripal::core
