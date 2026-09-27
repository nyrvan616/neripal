#include "neripal/core/Pet.hpp"

#include "neripal/core/Balance.hpp"
#include "neripal/core/Care.hpp"
#include "neripal/core/CareHistory.hpp"
#include "neripal/core/EvolutionRules.hpp"
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

bool Pet::advanceOneStage() {
    const auto from = state_.stage;
    bool hatched = false;
    switch (from) {
        case EvolutionStage::Egg:
            state_.stage = EvolutionStage::Baby;
            state_.form = FormId::Juvenile;
            hatched = true;
            break;
        case EvolutionStage::Baby:
            state_.stage = EvolutionStage::Child;
            if (state_.form == FormId::None) {
                state_.form = FormId::Juvenile;
            }
            break;
        case EvolutionStage::Child: {
            state_.stage = EvolutionStage::Adult;
            EvolutionContext context;
            context.care = &care_;
            context.leaving = EvolutionStage::Child;
            state_.form = resolveEvolution(context, random_);
            break;
        }
        case EvolutionStage::Adult:
            state_.stage = EvolutionStage::Final;
            break;
        case EvolutionStage::Final:
            return false;
    }
    EvolutionNotice notice;
    notice.from = from;
    notice.to = state_.stage;
    notice.form = state_.form;
    notices_.push(notice);
    return hatched;
}

bool Pet::advanceIfDue(bool liveAnnounce) {
    if (millisUntilNextStage() != 0) {
        return false;
    }
    const bool hatched = advanceOneStage();
    if (hatched && liveAnnounce) {
        pushHatched();
    }
    return true;
}

void Pet::stepMinute(bool tickCare, bool allowStage) {
    if (state_.stage != EvolutionStage::Egg) {
        applyNeedsStep();
        if (tickCare && !state_.sleeping) {
            tickCareEpisodes(care_, state_);
        }
    }
    if (allowStage) {
        advanceIfDue(tickCare);
    }
}

void Pet::pushHatched() noexcept {
    GameEvent event;
    event.kind = GameEventKind::Hatched;
    events_.push(event);
}

bool Pet::takeTimelineSlice(ClockBudgets& budget, bool tickCare, int sleepMode, std::uint64_t napLimit,
                            bool& blockFurtherStages) {
    const bool doAge = budget.age > 0;
    const bool doNeeds = budget.needs > 0;
    if (!doAge && !doNeeds) {
        return false;
    }

    if (doAge && millisUntilNextStage() == 0) {
        if (!blockFurtherStages) {
            advanceIfDue(tickCare);
            if (millisUntilNextStage() == 0) {
                blockFurtherStages = true;
            }
            return true;
        }
    }

    std::uint64_t chunk = std::numeric_limits<std::uint64_t>::max();
    if (doAge) {
        chunk = std::min(chunk, budget.age);
    }
    if (doNeeds) {
        chunk = std::min(chunk, budget.needs);
    }
    if (doAge && !blockFurtherStages) {
        const auto toStage = millisUntilNextStage();
        if (toStage > 0 && toStage < chunk) {
            chunk = toStage;
        }
    }
    if (doNeeds && state_.stage != EvolutionStage::Egg) {
        if (needsRemainderMs_ >= balance::kNeedsStepMs) {
            needsRemainderMs_ %= balance::kNeedsStepMs;
        }
        const auto toNeeds = balance::kNeedsStepMs - needsRemainderMs_;
        if (toNeeds < chunk) {
            chunk = toNeeds;
        }
    }
    if (napLimit < chunk) {
        chunk = napLimit;
    }
    if (chunk == 0 || chunk == std::numeric_limits<std::uint64_t>::max()) {
        return false;
    }

    if (doAge) {
        state_.ageMillis += chunk;
        budget.age -= chunk;
    }
    if (doNeeds) {
        budget.needs -= chunk;
        if (state_.stage != EvolutionStage::Egg) {
            needsRemainderMs_ += chunk;
        }
    }

    const bool minuteDone = doNeeds && state_.stage != EvolutionStage::Egg &&
                            needsRemainderMs_ >= balance::kNeedsStepMs;
    const bool allowStage = doAge && !blockFurtherStages;
    if (minuteDone) {
        needsRemainderMs_ -= balance::kNeedsStepMs;
        if (sleepMode >= 0) {
            state_.sleeping = sleepMode == 1;
        }
        stepMinute(tickCare, allowStage);
    } else if (allowStage) {
        advanceIfDue(tickCare);
    }
    if (doAge && millisUntilNextStage() != 0) {
        blockFurtherStages = false;
    }
    return true;
}

bool Pet::tryBulkStableMinutes(ClockBudgets& budget, bool sleeping) {
    constexpr std::uint64_t kProbeMinutes = balance::kNeedsPhaseCycle;
    constexpr std::uint64_t kProbeMs = kProbeMinutes * balance::kNeedsStepMs;
    const bool doAge = budget.age > 0;
    const bool doNeeds = budget.needs > 0;
    if (!doNeeds || state_.stage == EvolutionStage::Egg || needsRemainderMs_ != 0) {
        return false;
    }
    if (budget.needs < kProbeMs) {
        return false;
    }
    if (doAge && budget.age < kProbeMs) {
        return false;
    }
    if (doAge) {
        const auto toStage = millisUntilNextStage();
        if (toStage == 0 || toStage <= kProbeMs) {
            return false;
        }
    }

    const auto statsBefore = state_;
    const auto phaseBefore = needsStepPhase_;
    const auto stageBefore = state_.stage;
    bool block = false;
    ClockBudgets probe;
    probe.age = doAge ? kProbeMs : 0;
    probe.needs = kProbeMs;
    const int sleepMode = sleeping ? 1 : 0;
    advanceTimeline(probe, false, sleepMode, std::numeric_limits<std::uint64_t>::max(), block);
    const auto ageUsed = (doAge ? kProbeMs : 0) - probe.age;
    const auto needsUsed = kProbeMs - probe.needs;
    if (doAge) {
        budget.age -= ageUsed;
    }
    budget.needs -= needsUsed;
    if (ageUsed + needsUsed == 0 || probe.age > 0 || probe.needs > 0) {
        return ageUsed + needsUsed > 0;
    }

    const bool frozen = state_.hunger == statsBefore.hunger &&
                        state_.happiness == statsBefore.happiness &&
                        state_.energy == statsBefore.energy && state_.health == statsBefore.health &&
                        state_.hygiene == statsBefore.hygiene &&
                        state_.affection == statsBefore.affection &&
                        state_.stimulation == statsBefore.stimulation &&
                        needsStepPhase_ == phaseBefore && state_.stage == stageBefore;
    if (!frozen || state_.stage == EvolutionStage::Egg) {
        return true;
    }

    std::uint64_t minutes = budget.needs / balance::kNeedsStepMs;
    if (doAge) {
        minutes = std::min(minutes, budget.age / balance::kNeedsStepMs);
        const auto toStage = millisUntilNextStage();
        if (toStage == 0) {
            return true;
        }
        minutes = std::min(minutes, toStage / balance::kNeedsStepMs);
    }
    if (minutes == 0) {
        return true;
    }

    const auto span = minutes * balance::kNeedsStepMs;
    if (doAge) {
        state_.ageMillis += span;
        budget.age -= span;
    }
    budget.needs -= span;
    recordStageSteps(care_, state_, static_cast<std::uint32_t>(minutes));
    needsStepPhase_ = static_cast<std::uint8_t>(
        (static_cast<std::uint64_t>(needsStepPhase_) + (minutes % balance::kNeedsPhaseCycle)) %
        balance::kNeedsPhaseCycle);
    state_.sleeping = sleeping;
    advanceIfDue(false);
    return true;
}

void Pet::advanceTimeline(ClockBudgets& budget, bool tickCare, int sleepMode, std::uint64_t napLimit,
                          bool& blockFurtherStages) {
    const bool napLimited = napLimit != std::numeric_limits<std::uint64_t>::max();
    while (budget.age > 0 || budget.needs > 0) {
        if (napLimited && napLimit == 0) {
            return;
        }
        const auto ageBefore = budget.age;
        const auto needsBefore = budget.needs;
        const auto stageBefore = state_.stage;
        const auto limit = napLimited ? napLimit : std::numeric_limits<std::uint64_t>::max();
        if (!takeTimelineSlice(budget, tickCare, sleepMode, limit, blockFurtherStages)) {
            return;
        }
        const auto ageSpent = ageBefore - budget.age;
        const auto needsSpent = needsBefore > budget.needs ? needsBefore - budget.needs : 0;
        const auto wall = std::max(ageSpent, needsSpent);
        if (napLimited) {
            if (wall >= napLimit) {
                return;
            }
            napLimit -= wall;
        }
        if (wall == 0 && state_.stage == stageBefore) {
            return;
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
    ClockBudgets budget{elapsed, elapsed};
    bool blockFurtherStages = false;
    if (elapsed > 0 && millisUntilNextStage() == 0) {
        advanceIfDue(true);
        if (millisUntilNextStage() == 0) {
            blockFurtherStages = true;
        }
    }
    advanceTimeline(budget, true, -1, std::numeric_limits<std::uint64_t>::max(), blockFurtherStages);

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
    notices_.clear();
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
    notices_.clear();
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
    snapshot.form = state_.form;
    snapshot.sleepCause = autonomy_.sleepCause();
    snapshot.napRemainingMs = autonomy_.napRemainingMs(state_);
    snapshot.care = care_;
    snapshot.noticeCount = notices_.count();
    for (std::uint8_t i = 0; i < snapshot.noticeCount; ++i) {
        snapshot.notices[i] = notices_.at(i);
    }
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
    state_.form = snapshot.form;
    care_ = snapshot.care;
    needsRemainderMs_ = snapshot.needsRemainderMs;
    if (needsRemainderMs_ >= balance::kNeedsStepMs) {
        needsRemainderMs_ %= balance::kNeedsStepMs;
    }
    events_.clear();
    notices_.clear();
    const auto noticeCount = snapshot.noticeCount < kEvolutionNoticeCapacity
                                 ? snapshot.noticeCount
                                 : kEvolutionNoticeCapacity;
    for (std::uint8_t i = 0; i < noticeCount; ++i) {
        notices_.push(snapshot.notices[i]);
    }
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

    ClockBudgets budget{ageElapsedMs, needsElapsedMs};
    bool blockFurtherStages = false;
    // An age already past the next gate moves one stage, matching live time.
    // Further gates are crossed only by the minutes this gap actually adds.
    if (budget.age > 0 && millisUntilNextStage() == 0) {
        advanceIfDue(false);
        if (millisUntilNextStage() == 0) {
            blockFurtherStages = true;
        }
    }

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

    constexpr auto kUnlimited = std::numeric_limits<std::uint64_t>::max();
    while (budget.age > 0 || budget.needs > 0) {
        const auto ageBefore = budget.age;
        const auto needsBefore = budget.needs;
        const auto stageBefore = state_.stage;
        const auto remainderBefore = needsRemainderMs_;

        const bool inNap = cause == SleepCause::Nap && napLeft > 0;
        const bool sleepPhase = cause == SleepCause::Player || inNap;
        const int sleepMode = sleepPhase ? 1 : 0;
        const auto sliceLimit = inNap ? napLeft : kUnlimited;

        if (!inNap && !blockFurtherStages && tryBulkStableMinutes(budget, sleepPhase)) {
            if (budget.age == ageBefore && budget.needs == needsBefore &&
                state_.stage == stageBefore) {
                break;
            }
            continue;
        }

        if (!takeTimelineSlice(budget, false, sleepMode, sliceLimit, blockFurtherStages)) {
            break;
        }

        const auto ageSpent = ageBefore - budget.age;
        const auto needsSpent = needsBefore > budget.needs ? needsBefore - budget.needs : 0;
        const auto wall = std::max(ageSpent, needsSpent);
        if (inNap) {
            if (wall >= napLeft) {
                napLeft = 0;
                cause = SleepCause::None;
            } else {
                napLeft -= wall;
            }
            const bool completedMinute = stageBefore != EvolutionStage::Egg && needsSpent > 0 &&
                                         remainderBefore + needsSpent >= balance::kNeedsStepMs;
            if (completedMinute && cause == SleepCause::Nap &&
                state_.energy >= balance::kNapWakeEnergy) {
                endNap();
            }
        }

        if (wall == 0 && state_.stage == stageBefore) {
            break;
        }
    }

    const bool napEnded = startedNap && cause != SleepCause::Nap;
    const auto napRemaining = napLeft > 0xFFFFFFFFull ? 0xFFFFFFFFu
                                                      : static_cast<std::uint32_t>(napLeft);
    events_.clear();
    autonomy_.presentOffline(state_, cause, napRemaining, napEnded);
    autonomy_.onStatsChanged(state_, events_);
    anchorClock();
}

}  // namespace neripal::core
