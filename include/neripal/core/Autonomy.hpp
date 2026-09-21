#pragma once

#include "neripal/core/Activity.hpp"
#include "neripal/core/Care.hpp"
#include "neripal/core/GameEvent.hpp"
#include "neripal/core/IRandom.hpp"
#include "neripal/core/Mood.hpp"
#include "neripal/core/PetState.hpp"

#include <cstdint>

namespace neripal::core {

enum class SleepCause : std::uint8_t {
    None,
    Player,
    Nap,
};

// Single active activity: Idle/Walk selection, care reactions, mood one-shots, Nap.
class Autonomy {
public:
    // Home pose: center X, default facing, Idle or player Sleep from `sleeping`.
    // Does not consume RNG or emit events. Clears leftover catch-up and edge memory.
    void reset(PetState& state);

    bool frozenBySleep(const PetState& state) const noexcept;
    SleepCause sleepCause() const noexcept { return sleepCause_; }

    void enterSleep(PetState& state, GameEventQueue& events);
    void enterIdle(PetState& state, GameEventQueue& events);

    // Applied care starts Eat/Happy. Sleep/Wake are entered via enterSleep/enterIdle.
    // RejectedNoEnergy may start Tired; other rejections must not call this.
    void onCareApplied(CareAction action, PetState& state, GameEventQueue& events);
    void onRejectedNoEnergy(PetState& state, GameEventQueue& events);

    // Compare deriveMood against the previous value. Dirty/Annoyed flanks start
    // one-shots from Idle/Walk only. Call after stats change, before advance.
    void onStatsChanged(PetState& state, GameEventQueue& events);

    // Apply elapsed time with leftover catch-up. Frozen (Egg / player Sleep)
    // does not tick, does not store that elapsed as remainder, and does not emit.
    void advance(PetState& state, IRandom& rng, GameEventQueue& events,
                 std::uint64_t elapsedMs, bool frozen);

private:
    void beginIdle(PetState& state, IRandom* rng);
    void beginWalk(PetState& state, IRandom& rng);
    void beginNap(PetState& state, IRandom& rng);
    void beginTimed(PetState& state, Activity activity, std::uint32_t durationMs);
    void selectNext(PetState& state, IRandom& rng, GameEventQueue& events);
    void interruptInto(PetState& state, GameEventQueue& events, Activity activity,
                       std::uint32_t durationMs);
    void applyWalkMotion(PetState& state, std::uint32_t elapsedAfter) const;
    void finishNap(PetState& state);
    std::uint32_t sampleIdleDuration(IRandom& rng, const PetState& state);
    IdleVariant sampleIdleVariant(IRandom& rng, Mood mood);
    int wanderChancePercent(const PetState& state) const;

    std::uint64_t remainderMs_ = 0;
    bool lastWasWalk_ = false;
    bool skipNextNap_ = false;
    SleepCause sleepCause_ = SleepCause::None;
    Mood previousMood_ = Mood::Calm;
    bool previousMoodValid_ = false;
    int walkStartX_ = 0;
    std::uint32_t walkTotalPx_ = 0;
};

}  // namespace neripal::core
