#include "neripal/core/Pet.hpp"

#include "neripal/core/Balance.hpp"
#include "neripal/core/Care.hpp"
#include "neripal/core/CareHistory.hpp"
#include "neripal/core/Needs.hpp"

#include <algorithm>
#include <limits>

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

CareResult Pet::applyStatAction(CareAction action) {
    if (state_.stage == EvolutionStage::Egg) return CareResult::RejectedEgg;
    if (state_.sleeping) return CareResult::RejectedAsleep;
    const StatDelta delta = careEffect(action);
    if (delta.energy < 0 && state_.energy < -delta.energy) {
        autonomy_.onRejectedNoEnergy(state_, events_);
        return CareResult::RejectedNoEnergy;
    }
    state_.hunger = clampStat(state_.hunger + delta.hunger);
    state_.energy = clampStat(state_.energy + delta.energy);
    state_.hygiene = clampStat(state_.hygiene + delta.hygiene);
    state_.affection = clampStat(state_.affection + delta.affection);
    state_.stimulation = clampStat(state_.stimulation + delta.stimulation);
    state_.happiness = clampStat(state_.happiness + delta.happiness);
    state_.health = clampStat(state_.health + delta.health);
    autonomy_.onCareApplied(action, state_, events_);
    return CareResult::Applied;
}

CareResult Pet::feed() { return applyStatAction(CareAction::Feed); }

CareResult Pet::train() {
    const auto result = applyStatAction(CareAction::Train);
    if (result == CareResult::Applied) {
        noteTraining(care_, state_.stage);
    }
    return result;
}

CareResult Pet::sleep() {
    if (state_.stage == EvolutionStage::Egg) return CareResult::RejectedEgg;
    if (state_.sleeping) return CareResult::RejectedAlreadySleeping;
    state_.sleeping = true;
    autonomy_.enterSleep(state_, events_);
    return CareResult::Applied;
}

CareResult Pet::wake() {
    if (state_.stage == EvolutionStage::Egg) return CareResult::RejectedEgg;
    if (!state_.sleeping) return CareResult::RejectedAlreadyAwake;
    state_.sleeping = false;
    autonomy_.enterIdle(state_, events_);
    ensureUrgentEpisodes(care_, state_);
    return CareResult::Applied;
}

CareResult Pet::clean() { return applyStatAction(CareAction::Clean); }

CareResult Pet::pet() { return applyStatAction(CareAction::Pet); }

CareResult Pet::play() { return applyStatAction(CareAction::Play); }

CareResult Pet::apply(CareAction action) {
    switch (action) {
        case CareAction::Feed: return feed();
        case CareAction::Train: return train();
        case CareAction::Sleep: return sleep();
        case CareAction::Wake: return wake();
        case CareAction::Clean: return clean();
        case CareAction::Pet: return pet();
        case CareAction::Play: return play();
    }
    return CareResult::RejectedAlreadyAwake;
}

void Pet::applyNeedsStep() {
    if (state_.stage == EvolutionStage::Egg) return;
    neripal::core::applyNeedsStep(state_, needsStepPhase_);
    recordStageStep(care_, state_);
}

std::uint64_t Pet::millisUntilNextStage() const noexcept {
    std::uint64_t gate = 0;
    switch (state_.stage) {
        case EvolutionStage::Egg: gate = evolution::kEggHatchAgeMs; break;
        case EvolutionStage::Baby: gate = evolution::kChildAgeMs; break;
        case EvolutionStage::Child: gate = evolution::kAdultAgeMs; break;
        case EvolutionStage::Adult: gate = evolution::kFinalAgeMs; break;
        case EvolutionStage::Final:
            return std::numeric_limits<std::uint64_t>::max();
    }
    if (state_.ageMillis >= gate) return 0;
    return gate - state_.ageMillis;
}

bool Pet::advanceOneStage() noexcept {
    switch (state_.stage) {
        case EvolutionStage::Egg:
            state_.stage = EvolutionStage::Baby;
            return true;
        case EvolutionStage::Baby:
            state_.stage = EvolutionStage::Child;
            return false;
        case EvolutionStage::Child:
            state_.stage = EvolutionStage::Adult;
            return false;
        case EvolutionStage::Adult:
            state_.stage = EvolutionStage::Final;
            return false;
        case EvolutionStage::Final:
            return false;
    }
    return false;
}

void Pet::pushHatched() noexcept {
    GameEvent event;
    event.kind = GameEventKind::Hatched;
    events_.push(event);
}

void Pet::applyTimeOnCurrentStage(std::uint64_t elapsed) {
    if (elapsed == 0) return;
    if (state_.stage == EvolutionStage::Egg) {
        state_.ageMillis += elapsed;
        return;
    }

    std::uint64_t remaining = elapsed;
    while (remaining > 0) {
        if (needsRemainderMs_ >= balance::kNeedsStepMs) {
            needsRemainderMs_ %= balance::kNeedsStepMs;
        }
        const auto toNeeds = balance::kNeedsStepMs - needsRemainderMs_;
        const auto chunk = remaining < toNeeds ? remaining : toNeeds;
        state_.ageMillis += chunk;
        needsRemainderMs_ += chunk;
        remaining -= chunk;
        if (needsRemainderMs_ >= balance::kNeedsStepMs) {
            needsRemainderMs_ -= balance::kNeedsStepMs;
            applyNeedsStep();
            if (!state_.sleeping) {
                tickCareEpisodes(care_, state_);
            }
        }
    }
}

void Pet::consumeElapsed(std::uint64_t elapsed) {
    if (elapsed == 0) return;
    // A persisted age that is already past the next gate advances one stage.
    // Further gates wait for time that actually crosses them, so a stale age
    // cannot skip Baby or Child in the same call.
    if (millisUntilNextStage() == 0) {
        if (advanceOneStage()) pushHatched();
    }

    std::uint64_t remaining = elapsed;
    while (remaining > 0) {
        const auto toStage = millisUntilNextStage();
        if (toStage == 0) {
            applyTimeOnCurrentStage(remaining);
            return;
        }
        const auto chunk = remaining < toStage ? remaining : toStage;
        applyTimeOnCurrentStage(chunk);
        remaining -= chunk;
        if (millisUntilNextStage() == 0) {
            if (advanceOneStage()) pushHatched();
        }
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
    consumeElapsed(elapsed);

    autonomy_.onStatsChanged(state_, events_);
    const bool frozen = startedAsEgg || autonomy_.frozenBySleep(state_);
    autonomy_.advance(state_, random_, events_, elapsed, frozen);
    if (state_.stage != EvolutionStage::Egg && !state_.sleeping) {
        ensureUrgentEpisodes(care_, state_);
    }
}

bool Pet::pollEvent(GameEvent& out) noexcept {
    return events_.poll(out);
}

void Pet::reset() {
    state_ = PetState{};
    needsRemainderMs_ = 0;
    needsStepPhase_ = 0;
    care_.clear();
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
    state_.affection = clampStat(state_.affection);
    state_.stimulation = clampStat(state_.stimulation);
    needsRemainderMs_ = 0;
    needsStepPhase_ = 0;
    care_.clearEpisodes();
    events_.clear();
    autonomy_.reset(state_);
    anchorClock();
}

PetSnapshot Pet::capture() const {
    PetSnapshot snapshot;
    snapshot.hunger = state_.hunger;
    snapshot.happiness = state_.happiness;
    snapshot.energy = state_.energy;
    snapshot.health = state_.health;
    snapshot.hygiene = state_.hygiene;
    snapshot.affection = state_.affection;
    snapshot.stimulation = state_.stimulation;
    snapshot.ageMillis = state_.ageMillis;
    snapshot.needsStepPhase = needsStepPhase_;
    snapshot.needsRemainderMs = needsRemainderMs_ > 0xFFFFFFFFull
                                    ? 0xFFFFFFFFu
                                    : static_cast<std::uint32_t>(needsRemainderMs_);
    snapshot.stage = state_.stage;
    snapshot.sleepCause = autonomy_.sleepCause();
    snapshot.napRemainingMs = autonomy_.napRemainingMs(state_);
    snapshot.care = care_;
    return snapshot;
}

void Pet::restoreSnapshot(const PetSnapshot& snapshot) {
    state_ = PetState{};
    state_.hunger = clampStat(snapshot.hunger);
    state_.happiness = clampStat(snapshot.happiness);
    state_.energy = clampStat(snapshot.energy);
    state_.health = clampStat(snapshot.health);
    state_.hygiene = clampStat(snapshot.hygiene);
    state_.affection = clampStat(snapshot.affection);
    state_.stimulation = clampStat(snapshot.stimulation);
    state_.ageMillis = snapshot.ageMillis;
    needsStepPhase_ = snapshot.needsStepPhase;
    if (needsStepPhase_ >= balance::kNeedsPhaseCycle) {
        needsStepPhase_ = 0;
    }
    state_.stage = snapshot.stage;
    care_ = snapshot.care;
    needsRemainderMs_ = snapshot.needsRemainderMs;
    if (needsRemainderMs_ >= balance::kNeedsStepMs) {
        needsRemainderMs_ %= balance::kNeedsStepMs;
    }
    events_.clear();
    autonomy_.restoreSnapshot(state_, snapshot.sleepCause, snapshot.napRemainingMs);
    anchorClock();
}

void Pet::applyOffline(std::uint64_t ageElapsedMs, std::uint64_t needsElapsedMs) {
    if (ageElapsedMs > balance::kMaxAgeOfflineMs) {
        ageElapsedMs = balance::kMaxAgeOfflineMs;
    }
    if (needsElapsedMs > balance::kMaxNeedsOfflineMs) {
        needsElapsedMs = balance::kMaxNeedsOfflineMs;
    }

    state_.ageMillis += ageElapsedMs;

    const bool startedNap = autonomy_.sleepCause() == SleepCause::Nap;
    SleepCause cause = autonomy_.sleepCause();
    std::uint64_t napLeft = startedNap ? autonomy_.napRemainingMs(state_) : 0;

    auto endNap = [&]() {
        cause = SleepCause::None;
        napLeft = 0;
        state_.sleeping = false;
    };

    if (cause == SleepCause::Nap && state_.energy >= balance::kNapWakeEnergy) {
        endNap();
    }

    bool phaseStable = cause != SleepCause::Nap;
    std::uint32_t stableSteps = 0;
    std::uint64_t cursor = 0;

    while (state_.stage != EvolutionStage::Egg && cursor < needsElapsedMs) {
        if (phaseStable && stableSteps >= balance::kNeedsSettleSteps) {
            break;
        }
        if (needsRemainderMs_ >= balance::kNeedsStepMs) {
            needsRemainderMs_ %= balance::kNeedsStepMs;
        }

        const std::uint64_t untilStep = balance::kNeedsStepMs - needsRemainderMs_;
        std::uint64_t chunk = std::min(untilStep, needsElapsedMs - cursor);
        const bool inNap = cause == SleepCause::Nap && napLeft > 0;
        const bool sleepPhase = cause == SleepCause::Player || inNap;
        if (inNap && chunk > napLeft) {
            chunk = napLeft;
        }

        needsRemainderMs_ += chunk;
        cursor += chunk;
        if (inNap) {
            napLeft -= chunk;
            if (napLeft == 0) {
                cause = SleepCause::None;
                state_.sleeping = false;
            }
        }

        if (needsRemainderMs_ >= balance::kNeedsStepMs) {
            needsRemainderMs_ -= balance::kNeedsStepMs;
            state_.sleeping = sleepPhase;
            applyNeedsStep();
            if (phaseStable) {
                ++stableSteps;
            }
            if (sleepPhase && cause == SleepCause::Nap &&
                state_.energy >= balance::kNapWakeEnergy) {
                endNap();
            }
        }

        if (cause != SleepCause::Nap) {
            phaseStable = true;
        }
    }

    if (state_.stage != EvolutionStage::Egg && cursor < needsElapsedMs) {
        const std::uint64_t rest = needsElapsedMs - cursor;
        needsRemainderMs_ = (needsRemainderMs_ + rest) % balance::kNeedsStepMs;
    }

    const bool napEnded = startedNap && cause != SleepCause::Nap;
    const auto napRemaining = static_cast<std::uint32_t>(napLeft);
    events_.clear();
    autonomy_.presentOffline(state_, cause, napRemaining, napEnded);
    autonomy_.onStatsChanged(state_, events_);
    anchorClock();
}

}  // namespace neripal::core
