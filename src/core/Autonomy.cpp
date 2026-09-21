#include "neripal/core/Autonomy.hpp"

#include "neripal/core/Balance.hpp"

#include <algorithm>
#include <limits>

namespace neripal::core {
namespace {
void pushFinished(GameEventQueue& events, const PetState& state, bool completed) {
    GameEvent event;
    event.kind = GameEventKind::ActivityFinished;
    event.activity = state.activity;
    event.idleVariant = state.idleVariant;
    event.completed = completed;
    events.push(event);
}

void pushStarted(GameEventQueue& events, const PetState& state) {
    GameEvent event;
    event.kind = GameEventKind::ActivityStarted;
    event.activity = state.activity;
    event.idleVariant = state.idleVariant;
    event.completed = false;
    events.push(event);
}

void applySleepPose(PetState& state) {
    state.activity = Activity::Sleep;
    state.activityElapsedMs = 0;
    state.activityDurationMs = 0;
}

int clampX(int x) {
    return std::clamp(x, balance::kWalkMinX, balance::kWalkMaxX);
}
}  // namespace

void Autonomy::reset(PetState& state) {
    remainderMs_ = 0;
    lastWasWalk_ = false;
    skipNextNap_ = false;
    walkStartX_ = balance::kPetHomeX;
    walkTotalPx_ = 0;
    state.x = balance::kPetHomeX;
    state.facing = balance::kDefaultFacing;
    if (state.sleeping) {
        sleepCause_ = SleepCause::Player;
        applySleepPose(state);
    } else {
        sleepCause_ = SleepCause::None;
        beginIdle(state, nullptr);
    }
    previousMood_ = deriveMood(state);
    previousMoodValid_ = true;
}

bool Autonomy::frozenBySleep(const PetState& state) const noexcept {
    return state.sleeping && sleepCause_ != SleepCause::Nap;
}

void Autonomy::enterSleep(PetState& state, GameEventQueue& events) {
    pushFinished(events, state, false);
    remainderMs_ = 0;
    lastWasWalk_ = false;
    sleepCause_ = SleepCause::Player;
    applySleepPose(state);
    previousMood_ = deriveMood(state);
    previousMoodValid_ = true;
    pushStarted(events, state);
}

void Autonomy::enterIdle(PetState& state, GameEventQueue& events) {
    pushFinished(events, state, false);
    remainderMs_ = 0;
    lastWasWalk_ = false;
    sleepCause_ = SleepCause::None;
    beginIdle(state, nullptr);
    previousMood_ = deriveMood(state);
    previousMoodValid_ = true;
    pushStarted(events, state);
}

void Autonomy::onCareApplied(CareAction action, PetState& state, GameEventQueue& events) {
    switch (action) {
        case CareAction::Feed:
            interruptInto(state, events, Activity::Eat, balance::kEatDurationMs);
            break;
        case CareAction::Train:
        case CareAction::Clean:
            interruptInto(state, events, Activity::Happy, balance::kHappyDurationMs);
            break;
        case CareAction::Sleep:
        case CareAction::Wake:
            break;
    }
    previousMood_ = deriveMood(state);
    previousMoodValid_ = true;
}

void Autonomy::onRejectedNoEnergy(PetState& state, GameEventQueue& events) {
    if (state.activity == Activity::Tired) {
        return;
    }
    interruptInto(state, events, Activity::Tired, balance::kTiredDurationMs);
    previousMood_ = deriveMood(state);
    previousMoodValid_ = true;
}

void Autonomy::onStatsChanged(PetState& state, GameEventQueue& events) {
    const Mood now = deriveMood(state);
    const Mood prev = previousMood_;
    const bool hadPrev = previousMoodValid_;
    previousMood_ = now;
    previousMoodValid_ = true;

    if (!hadPrev) {
        return;
    }
    if (state.stage == EvolutionStage::Egg || state.sleeping) {
        return;
    }
    if (state.activity != Activity::Idle && state.activity != Activity::Walk) {
        return;
    }
    if (now == Mood::Dirty && prev != Mood::Dirty) {
        interruptInto(state, events, Activity::Dirty, balance::kDirtyDurationMs);
        return;
    }
    if (now == Mood::Annoyed && prev != Mood::Annoyed) {
        interruptInto(state, events, Activity::Annoyed, balance::kAnnoyedDurationMs);
    }
}

void Autonomy::beginIdle(PetState& state, IRandom* rng) {
    lastWasWalk_ = false;
    state.activity = Activity::Idle;
    state.activityElapsedMs = 0;
    if (rng == nullptr) {
        state.activityDurationMs = balance::kIdleDurationMinMs;
        state.idleVariant = IdleVariant::Bob;
        return;
    }
    state.activityDurationMs = sampleIdleDuration(*rng, state);
    state.idleVariant = sampleIdleVariant(*rng, deriveMood(state));
}

void Autonomy::beginTimed(PetState& state, Activity activity, std::uint32_t durationMs) {
    lastWasWalk_ = false;
    state.activity = activity;
    state.activityElapsedMs = 0;
    state.activityDurationMs = durationMs;
}

void Autonomy::beginWalk(PetState& state, IRandom& rng) {
    state.x = clampX(state.x);
    int dir = rng.nextBounded(2u) == 0u ? -1 : 1;
    if (state.x <= balance::kWalkMinX) {
        dir = 1;
    } else if (state.x >= balance::kWalkMaxX) {
        dir = -1;
    }
    state.facing = dir;

    const std::uint32_t span = static_cast<std::uint32_t>(
        balance::kWalkMaxDistancePx - balance::kWalkMinDistancePx);
    std::uint32_t dist =
        static_cast<std::uint32_t>(balance::kWalkMinDistancePx) + rng.nextBounded(span + 1u);
    const int space = dir > 0 ? balance::kWalkMaxX - state.x : state.x - balance::kWalkMinX;
    if (space <= 0) {
        beginIdle(state, &rng);
        return;
    }
    dist = std::min(dist, static_cast<std::uint32_t>(space));
    lastWasWalk_ = true;
    walkStartX_ = state.x;
    walkTotalPx_ = dist;
    state.activity = Activity::Walk;
    state.activityElapsedMs = 0;
    state.activityDurationMs = dist * balance::kWalkMsPerPixel;
}

void Autonomy::beginNap(PetState& state, IRandom& rng) {
    lastWasWalk_ = false;
    skipNextNap_ = false;
    sleepCause_ = SleepCause::Nap;
    state.sleeping = true;
    const std::uint32_t span = balance::kNapDurationMaxMs - balance::kNapDurationMinMs;
    beginTimed(state, Activity::Nap,
               balance::kNapDurationMinMs + rng.nextBounded(span + 1u));
}

void Autonomy::finishNap(PetState& state) {
    state.sleeping = false;
    sleepCause_ = SleepCause::None;
    skipNextNap_ = true;
}

void Autonomy::interruptInto(PetState& state, GameEventQueue& events, Activity activity,
                             std::uint32_t durationMs) {
    pushFinished(events, state, false);
    remainderMs_ = 0;
    lastWasWalk_ = false;
    if (sleepCause_ == SleepCause::Nap && !state.sleeping) {
        sleepCause_ = SleepCause::None;
    }
    beginTimed(state, activity, durationMs);
    pushStarted(events, state);
}

int Autonomy::wanderChancePercent(const PetState& state) const {
    int chance = balance::kWanderChancePercent;
    const Mood mood = deriveMood(state);
    if (mood == Mood::Annoyed) {
        chance += balance::kWanderAnnoyedBonusPercent;
    }
    if (state.energy >= balance::kIdleHighEnergyThreshold) {
        chance += balance::kWanderHighEnergyBonusPercent;
    }
    if (mood == Mood::Tired) {
        chance -= balance::kWanderTiredPenaltyPercent;
    }
    return std::clamp(chance, 0, 100);
}

void Autonomy::selectNext(PetState& state, IRandom& rng, GameEventQueue& events) {
    if (state.energy <= balance::kAutonomousNapEnergy && sleepCause_ != SleepCause::Player &&
        !skipNextNap_) {
        beginNap(state, rng);
        pushStarted(events, state);
        return;
    }
    skipNextNap_ = false;
    if (!lastWasWalk_) {
        const int chance = wanderChancePercent(state);
        if (static_cast<int>(rng.nextBounded(100u)) < chance) {
            beginWalk(state, rng);
            pushStarted(events, state);
            return;
        }
    }
    beginIdle(state, &rng);
    pushStarted(events, state);
}

void Autonomy::applyWalkMotion(PetState& state, std::uint32_t elapsedAfter) const {
    if (state.activity != Activity::Walk) {
        return;
    }
    const std::uint32_t pixels =
        std::min(walkTotalPx_, elapsedAfter / balance::kWalkMsPerPixel);
    state.x = clampX(walkStartX_ + state.facing * static_cast<int>(pixels));
}

std::uint32_t Autonomy::sampleIdleDuration(IRandom& rng, const PetState& state) {
    const std::uint32_t span =
        balance::kIdleDurationMaxMs - balance::kIdleDurationMinMs;
    std::uint32_t duration =
        balance::kIdleDurationMinMs + rng.nextBounded(span + 1u);

    if (state.energy <= balance::kTiredMoodEnergy) {
        duration += balance::kIdleTiredDurationBonusMs;
    } else if (state.energy >= balance::kIdleHighEnergyThreshold) {
        duration = balance::kIdleDurationMinMs +
                   (duration - balance::kIdleDurationMinMs) / 2u;
    }
    return duration;
}

IdleVariant Autonomy::sampleIdleVariant(IRandom& rng, Mood mood) {
    switch (mood) {
        case Mood::Happy: {
            constexpr IdleVariant kPool[] = {IdleVariant::Bob, IdleVariant::Bounce};
            return kPool[rng.nextBounded(2u)];
        }
        case Mood::Annoyed:
            (void)rng.nextBounded(1u);
            return IdleVariant::Fidget;
        case Mood::Tired:
            (void)rng.nextBounded(1u);
            return IdleVariant::Slump;
        case Mood::Dirty:
            (void)rng.nextBounded(1u);
            return IdleVariant::Shake;
        default: {
            constexpr IdleVariant kPool[] = {IdleVariant::Bob, IdleVariant::Glance};
            return kPool[rng.nextBounded(2u)];
        }
    }
}

void Autonomy::advance(PetState& state, IRandom& rng, GameEventQueue& events,
                       std::uint64_t elapsedMs, bool frozen) {
    if (frozen) {
        return;
    }

    elapsedMs += remainderMs_;
    remainderMs_ = 0;

    std::uint32_t transitions = 0;
    while (elapsedMs > 0 && transitions < balance::kMaxActivityTransitionsPerUpdate) {
        if (state.activity == Activity::Nap &&
            state.energy >= balance::kNapWakeEnergy) {
            pushFinished(events, state, true);
            finishNap(state);
            selectNext(state, rng, events);
            ++transitions;
            continue;
        }

        if (state.activityDurationMs == 0) {
            const std::uint64_t room = static_cast<std::uint64_t>(
                std::numeric_limits<std::uint32_t>::max() - state.activityElapsedMs);
            const std::uint64_t consumed = std::min(elapsedMs, room);
            state.activityElapsedMs += static_cast<std::uint32_t>(consumed);
            elapsedMs -= consumed;
            break;
        }

        const std::uint32_t remaining =
            state.activityDurationMs - state.activityElapsedMs;
        if (elapsedMs < remaining) {
            const auto nextElapsed =
                state.activityElapsedMs + static_cast<std::uint32_t>(elapsedMs);
            applyWalkMotion(state, nextElapsed);
            state.activityElapsedMs = nextElapsed;
            elapsedMs = 0;
            break;
        }

        applyWalkMotion(state, state.activityDurationMs);
        elapsedMs -= remaining;
        pushFinished(events, state, true);
        if (state.activity == Activity::Nap) {
            finishNap(state);
        }
        selectNext(state, rng, events);
        ++transitions;
    }

    remainderMs_ = elapsedMs;
}

}  // namespace neripal::core
