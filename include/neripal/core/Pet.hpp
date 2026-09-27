#pragma once

#include "neripal/core/Autonomy.hpp"
#include "neripal/core/Care.hpp"
#include "neripal/core/EvolutionNotice.hpp"
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
    const CareRecord& careRecord() const noexcept { return care_; }

    CareResult feed();
    CareResult train();
    CareResult sleep();
    CareResult wake();
    CareResult clean();
    CareResult pet();
    CareResult play();
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

    // Advance age, needs, stage history and stage crossings across a powered-off
    // gap. Does not read IClock for the gap and does not run Autonomy.
    // Care windows are not opened, advanced or expired. RNG is consumed only if
    // this gap resolves a Child -> Adult rule with two exits.
    // Age is capped by kMaxAgeOfflineMs. Needs are capped by kMaxNeedsOfflineMs.
    void applyOffline(std::uint64_t ageElapsedMs, std::uint64_t needsElapsedMs);

    std::uint8_t pendingEvolutionNotices() const noexcept { return notices_.count(); }
    bool peekEvolutionNotice(EvolutionNotice& out) const noexcept { return notices_.peek(out); }
    bool confirmEvolutionNotice() noexcept { return notices_.confirm(); }

private:
    static int clampStat(int value) noexcept;
    CareResult applyStatAction(CareAction action);
    struct ClockBudgets {
        std::uint64_t age = 0;
        std::uint64_t needs = 0;
    };

    void applyNeedsStep();
    std::uint64_t millisUntilNextStage() const noexcept;
    bool advanceOneStage();
    bool advanceIfDue(bool liveAnnounce);
    void stepMinute(bool tickCare, bool allowStage);
    bool takeTimelineSlice(ClockBudgets& budget, bool tickCare, int sleepMode, std::uint64_t napLimit,
                           bool& blockFurtherStages);
    bool tryBulkStableMinutes(ClockBudgets& budget, bool sleeping);
    void advanceTimeline(ClockBudgets& budget, bool tickCare, int sleepMode, std::uint64_t napLimit,
                         bool& blockFurtherStages);
    void pushHatched() noexcept;
    void anchorClock();

    IClock& clock_;
    IRandom& random_;
    Autonomy autonomy_{};
    GameEventQueue events_{};
    EvolutionNoticeQueue notices_{};
    PetState state_{};
    CareRecord care_{};
    std::uint64_t lastUpdateMs_ = 0;
    std::uint64_t needsRemainderMs_ = 0;
    std::uint8_t needsStepPhase_ = 0;
};

}  // namespace neripal::core

