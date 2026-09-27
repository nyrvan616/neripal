#pragma once

#include "neripal/core/Autonomy.hpp"
#include "neripal/core/Care.hpp"
#include "neripal/core/GameEvent.hpp"
#include "neripal/core/IClock.hpp"
#include "neripal/core/IRandom.hpp"
#include "neripal/core/PetSnapshot.hpp"
#include "neripal/core/PetState.hpp"

#include <cstdint>

namespace neripal::core {

class Pet {
public:
    Pet(IClock& clock, IRandom& random);

    const PetState& state() const noexcept { return state_; }

    CareResult feed();
    CareResult train();
    CareResult sleep();
    CareResult wake();
    CareResult clean();
    CareResult apply(CareAction action);
    void update();
    void reset();

    // Consumes the oldest queued event. Returns false if empty and leaves `out`
    // unchanged. Overflow already dropped the oldest events (capacity
    // kGameEventCapacity); the queue never grows.
    bool pollEvent(GameEvent& out) noexcept;

    // Debug restoration from a presentation snapshot. Zeros needs remainder and
    // infers Player Sleep only from `sleeping`. Persistence uses restoreSnapshot.
    void restore(const PetState& state);

    PetSnapshot capture() const;
    void restoreSnapshot(const PetSnapshot& snapshot);

    // Advance age and needs across a powered-off gap. Does not read IClock for
    // the gap, does not run Autonomy, and does not consume IRandom.
    // Age is capped by kMaxAgeOfflineMs (defensive, not a gameplay rule).
    // Needs are capped by kMaxNeedsOfflineMs.
    void applyOffline(std::uint64_t ageElapsedMs, std::uint64_t needsElapsedMs);

private:
    static int clampStat(int value) noexcept;
    void applyNeedsStep();
    void updateEvolution();
    void anchorClock();

    IClock& clock_;
    IRandom& random_;
    Autonomy autonomy_{};
    GameEventQueue events_{};
    PetState state_{};
    std::uint64_t lastUpdateMs_ = 0;
    std::uint64_t needsRemainderMs_ = 0;
};

}  // namespace neripal::core

