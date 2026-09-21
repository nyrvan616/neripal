#include "FakeClock.hpp"
#include "FakeRandom.hpp"
#include "neripal/core/Activity.hpp"
#include "neripal/core/Autonomy.hpp"
#include "neripal/core/Balance.hpp"
#include "neripal/core/Care.hpp"
#include "neripal/core/GameEvent.hpp"
#include "neripal/core/Mood.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/core/XorShift32.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
using neripal::core::Activity;
using neripal::core::CareResult;
using neripal::core::GameEvent;
using neripal::core::GameEventKind;
using neripal::core::GameEventQueue;
using neripal::core::IdleVariant;
using neripal::core::Pet;
using neripal::core::PetState;
using neripal::core::XorShift32;

struct TestCase { std::string_view name; std::function<bool()> run; };

bool sameCareStats(const PetState& a, const PetState& b) {
    return a.hunger == b.hunger && a.happiness == b.happiness && a.energy == b.energy &&
           a.health == b.health && a.hygiene == b.hygiene && a.sleeping == b.sleeping;
}

bool sameAutonomySnapshot(const PetState& a, const PetState& b) {
    return a.activity == b.activity && a.idleVariant == b.idleVariant && a.facing == b.facing &&
           a.x == b.x && a.activityElapsedMs == b.activityElapsedMs &&
           a.activityDurationMs == b.activityDurationMs;
}

FakeRandom zeroRng(std::size_t samples) {
    return FakeRandom(std::vector<std::uint32_t>(samples, 0u));
}

// Calm energy 80 → wanderP 20. nextBounded(100)==20 does not start Walk.
constexpr std::uint32_t kSkipWalk = 20;

FakeRandom skipWalkIdleRng(std::size_t reelections) {
    std::vector<std::uint32_t> samples;
    samples.reserve(reelections * 3);
    for (std::size_t i = 0; i < reelections; ++i) {
        samples.push_back(kSkipWalk);
        samples.push_back(0u);
        samples.push_back(0u);
    }
    return FakeRandom(std::move(samples));
}

bool feedReducesHunger() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng); const int before = pet.state().hunger;
    return pet.feed() == CareResult::Applied && pet.state().hunger < before;
}
bool hungerNeverBelowZero() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng); for (int i = 0; i < 20; ++i) pet.feed();
    return pet.state().hunger == 0;
}
bool trainingConsumesEnergy() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng); const int before = pet.state().energy;
    return pet.train() == CareResult::Applied && pet.state().energy < before;
}
bool sleepingRecoversEnergy() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng); auto state = pet.state(); state.energy = 50;
    pet.restore(state); pet.sleep(); clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update(); return pet.state().energy > 50;
}
bool energyNeverAbove100() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng); auto state = pet.state(); state.energy = 99;
    pet.restore(state); pet.sleep(); clock.advance(neripal::core::balance::kNeedsStepMs * 10);
    pet.update(); return pet.state().energy == 100;
}
bool happinessIsClamped() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng); auto state = pet.state(); state.happiness = 999;
    pet.restore(state); if (pet.state().happiness != 100) return false;
    state.happiness = -50; pet.restore(state); return pet.state().happiness == 0;
}
bool healthIsClamped() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng); auto state = pet.state(); state.health = -1;
    pet.restore(state); if (pet.state().health != 0) return false;
    state.health = 101; pet.restore(state); return pet.state().health == 100;
}
bool controlledTimeIncreasesHunger() {
    FakeClock clock; XorShift32 rng(1u); Pet pet(clock, rng); const int before = pet.state().hunger;
    clock.advance(neripal::core::balance::kNeedsStepMs); pet.update();
    return pet.state().hunger == before + neripal::core::balance::kHungerPerStep;
}
bool controlledTimeChangesEnergyByState() {
    FakeClock clock; XorShift32 rng(1u); Pet pet(clock, rng); const int awake = pet.state().energy;
    clock.advance(neripal::core::balance::kNeedsStepMs); pet.update();
    if (pet.state().energy >= awake) return false;
    const int beforeSleep = pet.state().energy; pet.sleep();
    clock.advance(neripal::core::balance::kNeedsStepMs); pet.update();
    return pet.state().energy > beforeSleep;
}
bool noRealWaitIsNeeded() {
    FakeClock clock; XorShift32 rng(1u); Pet pet(clock, rng);
    clock.advance(3 * neripal::core::balance::kNeedsStepMs); pet.update();
    return pet.state().ageMillis == 3 * neripal::core::balance::kNeedsStepMs;
}
bool restoreNormalizesEveryStat() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng);
    PetState invalid{};
    invalid.hunger = -5;
    invalid.happiness = 105;
    invalid.energy = -50;
    invalid.health = 800;
    invalid.hygiene = 200;
    invalid.ageMillis = 123;
    invalid.sleeping = true;
    pet.restore(invalid);
    const auto& state = pet.state();
    return state.hunger == 0 && state.happiness == 100 && state.energy == 0 &&
           state.health == 100 && state.hygiene == 100 && state.ageMillis == 123 &&
           state.sleeping;
}
bool hygieneRestoreClampsLow() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng);
    PetState invalid = pet.state();
    invalid.hygiene = -8;
    pet.restore(invalid);
    return pet.state().hygiene == 0;
}
bool evolutionUsesControlledAge() {
    FakeClock clock; XorShift32 rng(1u); Pet pet(clock, rng);
    clock.advance(neripal::core::evolution::kChildAgeMs); pet.update();
    if (pet.state().stage != neripal::core::EvolutionStage::Child) return false;
    clock.advance(neripal::core::evolution::kAdultAgeMs -
                  neripal::core::evolution::kChildAgeMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Adult;
}
bool eggHatchesWithControlledAge() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng); auto state = pet.state();
    state.stage = neripal::core::EvolutionStage::Egg;
    state.ageMillis = 0;
    pet.restore(state);
    if (pet.state().stage != neripal::core::EvolutionStage::Egg) return false;
    clock.advance(neripal::core::evolution::kEggHatchAgeMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Baby;
}
bool cleanRaisesHygiene() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng);
    auto state = pet.state();
    state.hygiene = 40;
    pet.restore(state);
    return pet.clean() == CareResult::Applied &&
           pet.state().hygiene == 40 + neripal::core::balance::kCleanHygiene;
}
bool cleanDoesNotExceedMax() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng);
    auto state = pet.state();
    state.hygiene = 90;
    pet.restore(state);
    return pet.clean() == CareResult::Applied && pet.state().hygiene == 100;
}
bool awakeTimeLowersHygiene() {
    FakeClock clock; XorShift32 rng(1u); Pet pet(clock, rng);
    const int before = pet.state().hygiene;
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().hygiene == before + neripal::core::balance::kHygienePerAwakeStep;
}
bool sleepDoesNotLowerHygiene() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng);
    auto state = pet.state();
    state.hygiene = 50;
    pet.restore(state);
    pet.sleep();
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().hygiene == 50;
}
bool highHungerLowersHappiness() {
    FakeClock clock; XorShift32 rng(1u); Pet pet(clock, rng);
    auto state = pet.state();
    state.hunger = neripal::core::balance::kNeglectThreshold;
    state.hygiene = 80;
    state.happiness = 50;
    pet.restore(state);
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().happiness == 49;
}
bool lowHygieneLowersHappiness() {
    FakeClock clock; XorShift32 rng(1u); Pet pet(clock, rng);
    auto state = pet.state();
    state.hunger = 10;
    state.hygiene = neripal::core::balance::kHygieneNeglectThreshold;
    state.happiness = 50;
    pet.restore(state);
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().happiness == 49;
}
bool zeroHygieneLowersHealth() {
    FakeClock clock; XorShift32 rng(1u); Pet pet(clock, rng);
    auto state = pet.state();
    state.hunger = 10;
    state.energy = 80;
    state.hygiene = 1;
    state.health = 50;
    pet.restore(state);
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().hygiene == 0 && pet.state().health == 49;
}
bool feedRejectedWhenAsleep() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng);
    auto state = pet.state();
    state.sleeping = true;
    pet.restore(state);
    const auto before = pet.state();
    return pet.feed() == CareResult::RejectedAsleep && sameCareStats(before, pet.state()) &&
           sameAutonomySnapshot(before, pet.state());
}
bool trainRejectedWhenAsleep() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng);
    auto state = pet.state();
    state.sleeping = true;
    state.energy = 5;
    pet.restore(state);
    const auto before = pet.state();
    return pet.train() == CareResult::RejectedAsleep && sameCareStats(before, pet.state());
}
bool cleanRejectedWhenAsleep() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng);
    auto state = pet.state();
    state.sleeping = true;
    pet.restore(state);
    const auto before = pet.state();
    return pet.clean() == CareResult::RejectedAsleep && sameCareStats(before, pet.state());
}
bool trainRejectedWithoutEnergy() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng);
    auto state = pet.state();
    state.energy = -neripal::core::balance::kTrainEnergy - 1;
    pet.restore(state);
    const auto before = pet.state();
    return pet.train() == CareResult::RejectedNoEnergy && sameCareStats(before, pet.state()) &&
           pet.state().activity == Activity::Tired &&
           pet.state().activityDurationMs == neripal::core::balance::kTiredDurationMs;
}
bool sleepRejectedWhenAlreadySleeping() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng);
    pet.sleep();
    const auto before = pet.state();
    return pet.sleep() == CareResult::RejectedAlreadySleeping &&
           sameCareStats(before, pet.state()) && sameAutonomySnapshot(before, pet.state());
}
bool wakeRejectedWhenAlreadyAwake() {
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng);
    const auto before = pet.state();
    return pet.wake() == CareResult::RejectedAlreadyAwake && sameCareStats(before, pet.state()) &&
           sameAutonomySnapshot(before, pet.state());
}
bool applyDispatchesCareActions() {
    using neripal::core::CareAction;
    FakeClock clock; FakeRandom rng; Pet pet(clock, rng);
    const int hungerBefore = pet.state().hunger;
    if (pet.apply(CareAction::Feed) != CareResult::Applied ||
        pet.state().hunger >= hungerBefore) {
        return false;
    }
    if (pet.apply(CareAction::Sleep) != CareResult::Applied || !pet.state().sleeping) {
        return false;
    }
    const auto asleep = pet.state();
    if (pet.apply(CareAction::Train) != CareResult::RejectedAsleep ||
        !sameCareStats(asleep, pet.state())) {
        return false;
    }
    if (pet.apply(CareAction::Wake) != CareResult::Applied || pet.state().sleeping) {
        return false;
    }
    auto dirty = pet.state();
    dirty.hygiene = 20;
    pet.restore(dirty);
    return pet.apply(CareAction::Clean) == CareResult::Applied &&
           pet.state().hygiene > 20;
}

bool xorShift32KnownSequence() {
    neripal::core::XorShift32 rng(1u);
    return rng.nextU32() == 270369u &&
           rng.nextU32() == 67634689u &&
           rng.nextU32() == 2647435461u &&
           rng.nextU32() == 307599695u;
}

bool xorShift32ZeroSeedIsNonZeroStream() {
    neripal::core::XorShift32 rng(0u);
    const auto a = rng.nextU32();
    const auto b = rng.nextU32();
    return a == 1359758873u && b == 3761132862u && a != 0u && b != 0u;
}

bool xorShift32SameSeedSameSequence() {
    neripal::core::XorShift32 a(0x4E455249u);
    neripal::core::XorShift32 b(0x4E455249u);
    for (int i = 0; i < 8; ++i) {
        if (a.nextU32() != b.nextU32()) return false;
    }
    return true;
}

bool fakeRandomPlaysScriptedValues() {
    FakeRandom rng({7u, 11u, 2u});
    return rng.nextU32() == 7u && rng.nextU32() == 11u && rng.nextU32() == 2u &&
           rng.remaining() == 0;
}

bool fakeRandomExhaustionFails() {
    FakeRandom rng({1u});
    if (rng.nextU32() != 1u) return false;
    try {
        (void)rng.nextU32();
        return false;
    } catch (const std::logic_error&) {
        return true;
    }
}

bool nextBoundedUsesQueuedSample() {
    FakeRandom rng({7u, 10u, 4u});
    return rng.nextBounded(4u) == 3u &&
           rng.nextBounded(10u) == 0u &&
           rng.nextBounded(3u) == 1u;
}

bool nextBoundedZeroDoesNotConsume() {
    FakeRandom rng({9u});
    if (rng.nextBounded(0u) != 0u || rng.remaining() != 1) return false;
    if (rng.nextBounded(1u) != 0u) return false;
    return rng.remaining() == 0;
}

bool constructorDoesNotConsumeRng() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    try {
        (void)rng.nextU32();
        return false;
    } catch (const std::logic_error&) {
        return pet.state().activity == Activity::Idle &&
               pet.state().activityDurationMs == neripal::core::balance::kIdleDurationMinMs &&
               pet.state().x == neripal::core::balance::kPetHomeX &&
               pet.state().facing == neripal::core::balance::kDefaultFacing;
    }
}

bool sleepingUpdateDoesNotConsumeRng() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    if (pet.sleep() != CareResult::Applied || pet.state().activity != Activity::Sleep) {
        return false;
    }
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    try {
        (void)rng.nextU32();
        return false;
    } catch (const std::logic_error&) {
        return pet.state().activity == Activity::Sleep && pet.state().activityElapsedMs == 0 &&
               pet.state().activityDurationMs == 0;
    }
}

template <typename T, typename = void>
struct hasMoodField : std::false_type {};

template <typename T>
struct hasMoodField<T, std::void_t<decltype(std::declval<T>().mood)>> : std::true_type {};

PetState calmBaseline() {
    PetState state;
    state.hunger = 40;
    state.happiness = 70;
    state.energy = 80;
    state.health = 100;
    state.hygiene = 80;
    state.sleeping = false;
    state.stage = neripal::core::EvolutionStage::Baby;
    return state;
}

bool petStateDoesNotCacheMood() {
    return !hasMoodField<PetState>::value;
}

bool deriveMoodFollowsPriorityTable() {
    using neripal::core::Mood;
    using neripal::core::deriveMood;
    namespace B = neripal::core::balance;

    if (deriveMood(calmBaseline()) != Mood::Calm) return false;

    PetState happy = calmBaseline();
    happy.happiness = B::kHappyMoodHappiness + 1;
    if (deriveMood(happy) != Mood::Happy) return false;
    happy.happiness = B::kHappyMoodHappiness;
    if (deriveMood(happy) != Mood::Calm) return false;

    PetState annoyedByHappiness = calmBaseline();
    annoyedByHappiness.happiness = B::kAnnoyedMoodHappiness;
    if (deriveMood(annoyedByHappiness) != Mood::Annoyed) return false;
    annoyedByHappiness.happiness = B::kAnnoyedMoodHappiness + 1;
    if (deriveMood(annoyedByHappiness) != Mood::Calm) return false;

    PetState annoyedByHunger = calmBaseline();
    annoyedByHunger.hunger = B::kAnnoyedMoodHunger;
    if (deriveMood(annoyedByHunger) != Mood::Annoyed) return false;
    annoyedByHunger.hunger = B::kAnnoyedMoodHunger - 1;
    if (deriveMood(annoyedByHunger) != Mood::Calm) return false;

    PetState annoyedByHealth = calmBaseline();
    annoyedByHealth.health = B::kAnnoyedMoodHealth - 1;
    if (deriveMood(annoyedByHealth) != Mood::Annoyed) return false;
    annoyedByHealth.health = B::kAnnoyedMoodHealth;
    if (deriveMood(annoyedByHealth) != Mood::Calm) return false;

    PetState dirty = calmBaseline();
    dirty.hygiene = B::kHygieneNeglectThreshold;
    if (deriveMood(dirty) != Mood::Dirty) return false;
    dirty.hygiene = B::kHygieneNeglectThreshold + 1;
    if (deriveMood(dirty) != Mood::Calm) return false;

    PetState tired = calmBaseline();
    tired.energy = B::kTiredMoodEnergy;
    if (deriveMood(tired) != Mood::Tired) return false;
    tired.energy = B::kTiredMoodEnergy + 1;
    if (deriveMood(tired) != Mood::Calm) return false;

    PetState resting = calmBaseline();
    resting.sleeping = true;
    if (deriveMood(resting) != Mood::Resting) return false;

    PetState sleepingOverrides = calmBaseline();
    sleepingOverrides.sleeping = true;
    sleepingOverrides.energy = 0;
    sleepingOverrides.hygiene = 0;
    sleepingOverrides.happiness = 0;
    if (deriveMood(sleepingOverrides) != Mood::Resting) return false;

    PetState tiredOverridesDirty = calmBaseline();
    tiredOverridesDirty.energy = B::kTiredMoodEnergy;
    tiredOverridesDirty.hygiene = 0;
    if (deriveMood(tiredOverridesDirty) != Mood::Tired) return false;

    PetState dirtyOverridesAnnoyed = calmBaseline();
    dirtyOverridesAnnoyed.hygiene = B::kHygieneNeglectThreshold;
    dirtyOverridesAnnoyed.happiness = 0;
    if (deriveMood(dirtyOverridesAnnoyed) != Mood::Dirty) return false;

    PetState hungerOverridesHappy = calmBaseline();
    hungerOverridesHappy.happiness = B::kHappyMoodHappiness + 1;
    hungerOverridesHappy.hunger = B::kAnnoyedMoodHunger;
    if (deriveMood(hungerOverridesHappy) != Mood::Annoyed) return false;

    PetState eggDoesNotOverride = calmBaseline();
    eggDoesNotOverride.stage = neripal::core::EvolutionStage::Egg;
    if (deriveMood(eggDoesNotOverride) != Mood::Calm) return false;
    eggDoesNotOverride.hygiene = B::kHygieneNeglectThreshold;
    if (deriveMood(eggDoesNotOverride) != Mood::Dirty) return false;
    eggDoesNotOverride.sleeping = true;
    if (deriveMood(eggDoesNotOverride) != Mood::Resting) return false;

    return true;
}

bool moodIsDerivedFromSnapshot() {
    using neripal::core::Mood;
    using neripal::core::deriveMood;

    PetState tired = calmBaseline();
    tired.energy = neripal::core::balance::kTiredMoodEnergy;
    PetState calm = tired;
    calm.energy = neripal::core::balance::kTiredMoodEnergy + 1;
    return deriveMood(tired) == Mood::Tired && deriveMood(calm) == Mood::Calm;
}

bool restoreResetsTransientActivity() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    PetState incoming = pet.state();
    incoming.x = 3;
    incoming.facing = -1;
    incoming.activity = Activity::Walk;
    incoming.idleVariant = IdleVariant::Shake;
    incoming.activityElapsedMs = 999;
    incoming.activityDurationMs = 50;
    incoming.sleeping = false;
    pet.restore(incoming);
    const auto& state = pet.state();
    return state.x == B::kPetHomeX && state.facing == B::kDefaultFacing &&
           state.activity == Activity::Idle && state.idleVariant == IdleVariant::Bob &&
           state.activityElapsedMs == 0 && state.activityDurationMs == B::kIdleDurationMinMs;
}

bool restoreSleepingStartsSleepActivity() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    PetState incoming = pet.state();
    incoming.sleeping = true;
    incoming.activity = Activity::Walk;
    incoming.x = 7;
    pet.restore(incoming);
    return pet.state().activity == Activity::Sleep && pet.state().activityDurationMs == 0 &&
           pet.state().activityElapsedMs == 0 &&
           pet.state().x == neripal::core::balance::kPetHomeX;
}

bool idleRngZeroGivesMinDuration() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({kSkipWalk, 0u, 0u});
    Pet pet(clock, rng);
    if (pet.state().activity != Activity::Idle ||
        pet.state().activityDurationMs != B::kIdleDurationMinMs) {
        return false;
    }
    clock.advance(B::kIdleDurationMinMs);
    pet.update();
    return pet.state().activity == Activity::Idle &&
           pet.state().activityDurationMs == B::kIdleDurationMinMs &&
           pet.state().idleVariant == IdleVariant::Bob && pet.state().activityElapsedMs == 0 &&
           rng.remaining() == 0;
}

bool idleLeftoverFeedsNextDecideTimer() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({kSkipWalk, 0u, 0u});
    Pet pet(clock, rng);
    clock.advance(B::kIdleDurationMinMs + 200);
    pet.update();
    return pet.state().activity == Activity::Idle &&
           pet.state().activityDurationMs == B::kIdleDurationMinMs &&
           pet.state().activityElapsedMs == 200 && rng.remaining() == 0;
}

bool idleRngSelectsMaxDurationAndGlance() {
    namespace B = neripal::core::balance;
    const auto span = B::kIdleDurationMaxMs - B::kIdleDurationMinMs;
    FakeClock clock;
    FakeRandom rng({kSkipWalk, span, 1u});
    Pet pet(clock, rng);
    clock.advance(B::kIdleDurationMinMs);
    pet.update();
    return pet.state().activityDurationMs == B::kIdleDurationMaxMs &&
           pet.state().idleVariant == IdleVariant::Glance && pet.state().activityElapsedMs == 0;
}

bool tiredIdleAddsDurationBonus() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    // Tired wanderP is 10; sample 10 skips Walk. Variant pool is Slump (nextBounded(1)).
    FakeRandom rng({10u, 0u, 0u});
    Pet pet(clock, rng);
    auto tired = pet.state();
    tired.energy = B::kTiredMoodEnergy;
    pet.restore(tired);
    clock.advance(B::kIdleDurationMinMs);
    pet.update();
    return pet.state().activityDurationMs == B::kIdleDurationMinMs + B::kIdleTiredDurationBonusMs &&
           pet.state().activityElapsedMs == 0 && pet.state().idleVariant == IdleVariant::Slump;
}

bool eggFreezesIdleDecideTimer() {
    FakeClock clock;
    FakeRandom rng({9u, 8u, 7u, 6u});
    Pet pet(clock, rng);
    auto egg = pet.state();
    egg.stage = neripal::core::EvolutionStage::Egg;
    egg.ageMillis = 0;
    pet.restore(egg);
    const auto before = pet.state();
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Egg &&
           sameAutonomySnapshot(before, pet.state()) && rng.remaining() == 4;
}

bool sleepFreezesIdleDecideTimer() {
    FakeClock clock;
    FakeRandom rng({9u, 8u, 7u, 6u});
    Pet pet(clock, rng);
    if (pet.sleep() != CareResult::Applied) return false;
    clock.advance(10 * neripal::core::balance::kIdleDurationMinMs);
    pet.update();
    return pet.state().activity == Activity::Sleep && pet.state().activityElapsedMs == 0 &&
           pet.state().activityDurationMs == 0 && rng.remaining() == 4;
}

bool wakeResumesIdleWithoutRng() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    if (pet.sleep() != CareResult::Applied) return false;
    if (pet.wake() != CareResult::Applied) return false;
    return pet.state().activity == Activity::Idle &&
           pet.state().activityDurationMs == neripal::core::balance::kIdleDurationMinMs &&
           pet.state().activityElapsedMs == 0;
}

bool stepSizeDoesNotChangeAutonomy() {
    namespace B = neripal::core::balance;
    const std::uint64_t total = 10'000;
    constexpr std::size_t kReelects = 16;

    FakeClock oneShotClock;
    FakeRandom oneShotRng = skipWalkIdleRng(kReelects);
    Pet oneShot(oneShotClock, oneShotRng);
    oneShotClock.advance(total);
    oneShot.update();

    FakeClock equalClock;
    FakeRandom equalRng = skipWalkIdleRng(kReelects);
    Pet equalSteps(equalClock, equalRng);
    for (int i = 0; i < 10; ++i) {
        equalClock.advance(1000);
        equalSteps.update();
    }

    FakeClock irregularClock;
    FakeRandom irregularRng = skipWalkIdleRng(kReelects);
    Pet irregular(irregularClock, irregularRng);
    irregularClock.advance(3000);
    irregular.update();
    irregularClock.advance(100);
    irregular.update();
    irregularClock.advance(6900);
    irregular.update();

    return sameAutonomySnapshot(oneShot.state(), equalSteps.state()) &&
           sameAutonomySnapshot(oneShot.state(), irregular.state()) &&
           oneShot.state().activity == Activity::Idle &&
           oneShot.state().x == B::kPetHomeX &&
           oneShot.state().facing == B::kDefaultFacing;
}

bool catchUpRemainderSurvivesTransitionCap() {
    namespace B = neripal::core::balance;
    const auto minD = B::kIdleDurationMinMs;
    const auto maxT = B::kMaxActivityTransitionsPerUpdate;
    const std::uint64_t extra = 400;
    const std::uint64_t total = static_cast<std::uint64_t>(maxT) * minD + extra;

    FakeClock clock;
    FakeRandom rng = skipWalkIdleRng(16);
    Pet pet(clock, rng);
    clock.advance(total);
    pet.update();
    if (pet.state().activity != Activity::Idle || pet.state().activityElapsedMs != 0 ||
        pet.state().activityDurationMs != minD) {
        return false;
    }
    const auto remainingAfterCap = rng.remaining();
    pet.update();
    return pet.state().activityElapsedMs == extra && pet.state().activityDurationMs == minD &&
           rng.remaining() == remainingAfterCap;
}

bool matchesEvent(const GameEvent& event, GameEventKind kind, Activity activity,
                  bool completed, IdleVariant variant) {
    return event.kind == kind && event.activity == activity && event.completed == completed &&
           event.idleVariant == variant;
}

std::size_t drainEvents(Pet& pet, GameEvent* out, std::size_t maxCount) {
    std::size_t count = 0;
    GameEvent event;
    while (count < maxCount && pet.pollEvent(event)) {
        if (out != nullptr) {
            out[count] = event;
        }
        ++count;
    }
    return count;
}

bool pollEventConsumesOnce() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({kSkipWalk, 0u, 0u});
    Pet pet(clock, rng);

    GameEvent leftover{};
    leftover.kind = GameEventKind::ActivityFinished;
    leftover.activity = Activity::Sleep;
    leftover.completed = true;
    if (pet.pollEvent(leftover)) return false;
    if (leftover.kind != GameEventKind::ActivityFinished || leftover.activity != Activity::Sleep ||
        !leftover.completed) {
        return false;
    }

    clock.advance(B::kIdleDurationMinMs);
    pet.update();

    GameEvent finished{};
    if (!pet.pollEvent(finished) ||
        !matchesEvent(finished, GameEventKind::ActivityFinished, Activity::Idle, true,
                      IdleVariant::Bob)) {
        return false;
    }
    GameEvent started{};
    if (!pet.pollEvent(started) ||
        !matchesEvent(started, GameEventKind::ActivityStarted, Activity::Idle, false,
                      IdleVariant::Bob)) {
        return false;
    }

    GameEvent again = finished;
    if (pet.pollEvent(again)) return false;
    if (again.kind != finished.kind || again.activity != finished.activity ||
        again.completed != finished.completed) {
        return false;
    }
    return !pet.pollEvent(again);
}

bool gameEventQueueIsFixedAndDropsOldest() {
    GameEventQueue queue;
    for (std::uint8_t i = 0; i < 10; ++i) {
        GameEvent event;
        event.kind = GameEventKind::ActivityStarted;
        event.activity = Activity::Idle;
        event.idleVariant = (i % 2u == 0u) ? IdleVariant::Bob : IdleVariant::Glance;
        event.completed = false;
        queue.push(event);
        if (queue.size() > neripal::core::kGameEventCapacity) return false;
    }
    if (queue.size() != neripal::core::kGameEventCapacity) return false;

    GameEvent events[8]{};
    std::size_t count = 0;
    GameEvent event;
    while (queue.poll(event)) {
        if (count >= 8) return false;
        events[count++] = event;
    }
    if (count != neripal::core::kGameEventCapacity) return false;
    if (queue.poll(event) || queue.size() != 0) return false;

    // 10 pushes of alternating Bob/Glance; last four are indices 6..9 = Bob, Glance, Bob, Glance.
    return events[0].idleVariant == IdleVariant::Bob &&
           events[1].idleVariant == IdleVariant::Glance &&
           events[2].idleVariant == IdleVariant::Bob &&
           events[3].idleVariant == IdleVariant::Glance;
}

bool petEventBufferDoesNotGrowPastCapacity() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({kSkipWalk, 0u, 0u, kSkipWalk, 0u, 1u, kSkipWalk, 0u, 0u});
    Pet pet(clock, rng);
    clock.advance(3 * B::kIdleDurationMinMs);
    pet.update();

    GameEvent events[8]{};
    const auto count = drainEvents(pet, events, 8);
    GameEvent extra{};
    extra.kind = GameEventKind::ActivityFinished;
    if (count != neripal::core::kGameEventCapacity || pet.pollEvent(extra)) return false;

    // Three Idle re-elections emit six edges; the oldest two are dropped.
    return matchesEvent(events[0], GameEventKind::ActivityFinished, Activity::Idle, true,
                        IdleVariant::Bob) &&
           matchesEvent(events[1], GameEventKind::ActivityStarted, Activity::Idle, false,
                        IdleVariant::Glance) &&
           matchesEvent(events[2], GameEventKind::ActivityFinished, Activity::Idle, true,
                        IdleVariant::Glance) &&
           matchesEvent(events[3], GameEventKind::ActivityStarted, Activity::Idle, false,
                        IdleVariant::Bob);
}

bool sleepWakeEmitUncompletedInterrupt() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    if (drainEvents(pet, nullptr, 4) != 0) return false;

    if (pet.sleep() != CareResult::Applied) return false;
    GameEvent finished{};
    GameEvent started{};
    if (!pet.pollEvent(finished) ||
        !matchesEvent(finished, GameEventKind::ActivityFinished, Activity::Idle, false,
                      IdleVariant::Bob)) {
        return false;
    }
    if (!pet.pollEvent(started) ||
        !matchesEvent(started, GameEventKind::ActivityStarted, Activity::Sleep, false,
                      IdleVariant::Bob)) {
        return false;
    }
    if (pet.pollEvent(finished)) return false;

    if (pet.wake() != CareResult::Applied) return false;
    if (!pet.pollEvent(finished) ||
        !matchesEvent(finished, GameEventKind::ActivityFinished, Activity::Sleep, false,
                      IdleVariant::Bob)) {
        return false;
    }
    if (!pet.pollEvent(started) ||
        !matchesEvent(started, GameEventKind::ActivityStarted, Activity::Idle, false,
                      IdleVariant::Bob)) {
        return false;
    }
    return !pet.pollEvent(finished);
}

bool rejectedCareDoesNotEmitEvents() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    const auto before = pet.state();
    if (pet.wake() != CareResult::RejectedAlreadyAwake || !sameCareStats(before, pet.state())) {
        return false;
    }
    GameEvent event{};
    if (pet.pollEvent(event)) return false;

    if (pet.sleep() != CareResult::Applied) return false;
    if (drainEvents(pet, nullptr, 4) != 2) return false;
    if (pet.sleep() != CareResult::RejectedAlreadySleeping) return false;
    if (pet.feed() != CareResult::RejectedAsleep) return false;
    return !pet.pollEvent(event);
}

bool frozenIdleDoesNotEmitEvents() {
    FakeClock clock;
    FakeRandom rng({9u, 8u, 7u, 6u});
    Pet pet(clock, rng);
    auto egg = pet.state();
    egg.stage = neripal::core::EvolutionStage::Egg;
    egg.ageMillis = 0;
    pet.restore(egg);
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    GameEvent event{};
    if (pet.pollEvent(event)) return false;

    pet.reset();
    if (pet.sleep() != CareResult::Applied) return false;
    if (drainEvents(pet, nullptr, 4) != 2) return false;
    clock.advance(10 * neripal::core::balance::kIdleDurationMinMs);
    pet.update();
    return !pet.pollEvent(event);
}

bool restoreClearsQueuedEvents() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({kSkipWalk, 0u, 0u});
    Pet pet(clock, rng);
    clock.advance(B::kIdleDurationMinMs);
    pet.update();
    GameEvent event{};
    if (!pet.pollEvent(event)) return false;

    PetState incoming = pet.state();
    pet.restore(incoming);
    return !pet.pollEvent(event);
}

bool walkUsesScriptedFacingAndDistance() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({0u, 0u, 0u});
    Pet pet(clock, rng);
    clock.advance(B::kIdleDurationMinMs);
    pet.update();
    const auto walkMs = static_cast<std::uint32_t>(B::kWalkMinDistancePx) * B::kWalkMsPerPixel;
    return pet.state().activity == Activity::Walk && pet.state().facing == -1 &&
           pet.state().x == B::kPetHomeX && pet.state().activityElapsedMs == 0 &&
           pet.state().activityDurationMs == walkMs && rng.remaining() == 0;
}

bool walkAdvancesXDeterministically() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({0u, 0u, 0u});
    Pet pet(clock, rng);
    const auto walkMs = static_cast<std::uint32_t>(B::kWalkMinDistancePx) * B::kWalkMsPerPixel;
    clock.advance(B::kIdleDurationMinMs + walkMs / 2);
    pet.update();
    const int expectedX = B::kPetHomeX - (B::kWalkMinDistancePx / 2);
    return pet.state().activity == Activity::Walk && pet.state().x == expectedX &&
           pet.state().facing == -1 &&
           pet.state().activityElapsedMs == walkMs / 2;
}

bool walkCompletesToIdleWithoutWanderRoll() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({0u, 0u, 0u, 0u, 0u});
    Pet pet(clock, rng);
    const auto walkMs = static_cast<std::uint32_t>(B::kWalkMinDistancePx) * B::kWalkMsPerPixel;
    clock.advance(B::kIdleDurationMinMs + walkMs);
    pet.update();
    return pet.state().activity == Activity::Idle &&
           pet.state().x == B::kPetHomeX - B::kWalkMinDistancePx &&
           pet.state().facing == -1 && pet.state().activityElapsedMs == 0 &&
           pet.state().activityDurationMs == B::kIdleDurationMinMs &&
           pet.state().idleVariant == IdleVariant::Bob && rng.remaining() == 0;
}

bool walkStepSizeDoesNotChangePosition() {
    namespace B = neripal::core::balance;
    const std::uint64_t total = 10'000;
    constexpr std::size_t kSamples = 32;

    FakeClock oneShotClock;
    FakeRandom oneShotRng = zeroRng(kSamples);
    Pet oneShot(oneShotClock, oneShotRng);
    oneShotClock.advance(total);
    oneShot.update();

    FakeClock equalClock;
    FakeRandom equalRng = zeroRng(kSamples);
    Pet equalSteps(equalClock, equalRng);
    for (int i = 0; i < 20; ++i) {
        equalClock.advance(500);
        equalSteps.update();
    }

    FakeClock irregularClock;
    FakeRandom irregularRng = zeroRng(kSamples);
    Pet irregular(irregularClock, irregularRng);
    irregularClock.advance(2500);
    irregular.update();
    irregularClock.advance(50);
    irregular.update();
    irregularClock.advance(7450);
    irregular.update();

    return sameAutonomySnapshot(oneShot.state(), equalSteps.state()) &&
           sameAutonomySnapshot(oneShot.state(), irregular.state()) &&
           oneShot.state().x >= B::kWalkMinX && oneShot.state().x <= B::kWalkMaxX;
}

bool eggDoesNotWalk() {
    FakeClock clock;
    FakeRandom rng = zeroRng(8);
    Pet pet(clock, rng);
    auto egg = pet.state();
    egg.stage = neripal::core::EvolutionStage::Egg;
    egg.ageMillis = 0;
    pet.restore(egg);
    const auto before = pet.state();
    clock.advance(neripal::core::balance::kIdleDurationMinMs * 4);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Egg &&
           pet.state().activity == Activity::Idle && pet.state().x == before.x &&
           rng.remaining() == 8;
}

bool feedAppliedOnWalkStartsEat() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({0u, 0u, 0u});
    Pet pet(clock, rng);
    clock.advance(B::kIdleDurationMinMs + 100);
    pet.update();
    if (pet.state().activity != Activity::Walk) return false;
    const int x = pet.state().x;
    if (pet.feed() != CareResult::Applied) return false;
    return pet.state().activity == Activity::Eat &&
           pet.state().activityDurationMs == B::kEatDurationMs &&
           pet.state().activityElapsedMs == 0 && pet.state().x == x;
}

bool eatCompletesToIdle() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({kSkipWalk, 0u, 0u});
    Pet pet(clock, rng);
    if (pet.feed() != CareResult::Applied || pet.state().activity != Activity::Eat) return false;
    clock.advance(B::kEatDurationMs);
    pet.update();
    return pet.state().activity == Activity::Idle && pet.state().activityElapsedMs == 0;
}

bool trainAndCleanAppliedStartHappy() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    if (pet.train() != CareResult::Applied || pet.state().activity != Activity::Happy) {
        return false;
    }
    pet.reset();
    auto dirty = pet.state();
    dirty.hygiene = 20;
    pet.restore(dirty);
    return pet.clean() == CareResult::Applied && pet.state().activity == Activity::Happy &&
           pet.state().activityDurationMs == B::kHappyDurationMs;
}

bool rejectedNoEnergyDoesNotRestartTired() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    auto low = pet.state();
    low.energy = -B::kTrainEnergy - 1;
    pet.restore(low);
    if (pet.train() != CareResult::RejectedNoEnergy || pet.state().activity != Activity::Tired) {
        return false;
    }
    clock.advance(200);
    pet.update();
    const auto elapsed = pet.state().activityElapsedMs;
    if (pet.train() != CareResult::RejectedNoEnergy) return false;
    return pet.state().activity == Activity::Tired && pet.state().activityElapsedMs == elapsed;
}

bool rejectedAlreadyAwakeKeepsIdle() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    const auto before = pet.state();
    return pet.wake() == CareResult::RejectedAlreadyAwake &&
           sameAutonomySnapshot(before, pet.state());
}

bool hygieneFlankStartsDirtyOnce() {
    using neripal::core::Autonomy;
    using neripal::core::GameEventQueue;
    namespace B = neripal::core::balance;

    PetState state = calmBaseline();
    state.hygiene = B::kHygieneNeglectThreshold + 1;
    Autonomy autonomy;
    GameEventQueue events;
    autonomy.reset(state);

    state.hygiene = B::kHygieneNeglectThreshold;
    autonomy.onStatsChanged(state, events);
    if (state.activity != Activity::Dirty) return false;

    GameEvent first{};
    bool started = false;
    GameEvent event;
    while (events.poll(event)) {
        if (event.kind == GameEventKind::ActivityStarted && event.activity == Activity::Dirty) {
            started = true;
        }
        first = event;
    }
    if (!started) return false;

    autonomy.onStatsChanged(state, events);
    while (events.poll(event)) {
        if (event.kind == GameEventKind::ActivityStarted && event.activity == Activity::Dirty) {
            return false;
        }
    }
    return state.activity == Activity::Dirty && first.activity == Activity::Dirty;
}

bool annoyedFlankStartsAnnoyed() {
    using neripal::core::Autonomy;
    using neripal::core::GameEventQueue;
    namespace B = neripal::core::balance;

    PetState state = calmBaseline();
    state.happiness = B::kAnnoyedMoodHappiness + 1;
    Autonomy autonomy;
    GameEventQueue events;
    autonomy.reset(state);

    state.happiness = B::kAnnoyedMoodHappiness;
    autonomy.onStatsChanged(state, events);
    if (state.activity != Activity::Annoyed) return false;

    bool started = false;
    GameEvent event;
    while (events.poll(event)) {
        if (event.kind == GameEventKind::ActivityStarted && event.activity == Activity::Annoyed) {
            started = true;
        }
    }
    if (!started) return false;

    autonomy.onStatsChanged(state, events);
    while (events.poll(event)) {
        if (event.kind == GameEventKind::ActivityStarted && event.activity == Activity::Annoyed) {
            return false;
        }
    }
    return true;
}

bool restoreDoesNotFireMoodOneShot() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    auto dirty = pet.state();
    dirty.hygiene = 0;
    pet.restore(dirty);
    return pet.state().activity == Activity::Idle &&
           neripal::core::deriveMood(pet.state()) == neripal::core::Mood::Dirty;
}

bool napStartsWhenEnergyCritical() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({0u});
    Pet pet(clock, rng);
    auto tired = pet.state();
    tired.energy = B::kAutonomousNapEnergy;
    pet.restore(tired);
    clock.advance(B::kIdleDurationMinMs);
    pet.update();
    return pet.state().activity == Activity::Nap && pet.state().sleeping &&
           pet.state().activityDurationMs == B::kNapDurationMinMs && rng.remaining() == 0;
}

bool napAutoWakesToIdle() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({0u, kSkipWalk, 0u, 0u});
    Pet pet(clock, rng);
    auto tired = pet.state();
    tired.energy = B::kAutonomousNapEnergy;
    pet.restore(tired);
    clock.advance(B::kIdleDurationMinMs + B::kNapDurationMinMs);
    pet.update();
    return pet.state().activity == Activity::Idle && !pet.state().sleeping &&
           pet.state().activityElapsedMs == 0 && rng.remaining() == 0;
}

bool napDoesNotWakeWhileEnergyLow() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({0u});
    Pet pet(clock, rng);
    auto tired = pet.state();
    tired.energy = B::kAutonomousNapEnergy;
    pet.restore(tired);
    clock.advance(B::kIdleDurationMinMs);
    pet.update();
    if (pet.state().activity != Activity::Nap) return false;
    clock.advance(1000);
    pet.update();
    return pet.state().activity == Activity::Nap && pet.state().sleeping &&
           pet.state().energy == B::kAutonomousNapEnergy;
}

bool feedRejectedDuringNapKeepsNap() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({0u});
    Pet pet(clock, rng);
    auto tired = pet.state();
    tired.energy = B::kAutonomousNapEnergy;
    pet.restore(tired);
    clock.advance(B::kIdleDurationMinMs);
    pet.update();
    if (pet.state().activity != Activity::Nap) return false;
    const auto before = pet.state();
    return pet.feed() == CareResult::RejectedAsleep &&
           sameAutonomySnapshot(before, pet.state()) && pet.state().sleeping;
}

bool playerSleepBlocksNapAndWalk() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng = zeroRng(8);
    Pet pet(clock, rng);
    auto low = pet.state();
    low.energy = B::kAutonomousNapEnergy;
    pet.restore(low);
    if (pet.sleep() != CareResult::Applied) return false;
    clock.advance(B::kIdleDurationMinMs * 4);
    pet.update();
    return pet.state().activity == Activity::Sleep && pet.state().sleeping &&
           rng.remaining() == 8;
}

bool playerWakeEndsNap() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({0u});
    Pet pet(clock, rng);
    auto tired = pet.state();
    tired.energy = B::kAutonomousNapEnergy;
    pet.restore(tired);
    clock.advance(B::kIdleDurationMinMs);
    pet.update();
    if (pet.state().activity != Activity::Nap) return false;
    return pet.wake() == CareResult::Applied && !pet.state().sleeping &&
           pet.state().activity == Activity::Idle;
}

bool happyIdleUsesBouncePool() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({kSkipWalk, 0u, 1u});
    Pet pet(clock, rng);
    auto happy = pet.state();
    happy.happiness = B::kHappyMoodHappiness + 1;
    pet.restore(happy);
    clock.advance(B::kIdleDurationMinMs);
    pet.update();
    return pet.state().idleVariant == IdleVariant::Bounce;
}
}

int main() {
    const std::vector<TestCase> tests{
        {"feed reduces hunger", feedReducesHunger},
        {"hunger never drops below zero", hungerNeverBelowZero},
        {"training consumes energy", trainingConsumesEnergy},
        {"sleeping recovers energy", sleepingRecoversEnergy},
        {"energy never exceeds 100", energyNeverAbove100},
        {"happiness stays in range", happinessIsClamped},
        {"health stays in range", healthIsClamped},
        {"controlled time increases hunger", controlledTimeIncreasesHunger},
        {"controlled time changes energy", controlledTimeChangesEnergyByState},
        {"controlled time needs no real wait", noRealWaitIsNeeded},
        {"restored state is normalized", restoreNormalizesEveryStat},
        {"hygiene restore clamps low", hygieneRestoreClampsLow},
        {"evolution uses controlled age", evolutionUsesControlledAge},
        {"egg hatches with controlled age", eggHatchesWithControlledAge},
        {"clean raises hygiene", cleanRaisesHygiene},
        {"clean does not exceed max", cleanDoesNotExceedMax},
        {"awake time lowers hygiene", awakeTimeLowersHygiene},
        {"sleep does not lower hygiene", sleepDoesNotLowerHygiene},
        {"high hunger lowers happiness", highHungerLowersHappiness},
        {"low hygiene lowers happiness", lowHygieneLowersHappiness},
        {"zero hygiene lowers health", zeroHygieneLowersHealth},
        {"feed rejected when asleep", feedRejectedWhenAsleep},
        {"train rejected when asleep", trainRejectedWhenAsleep},
        {"clean rejected when asleep", cleanRejectedWhenAsleep},
        {"train rejected without energy", trainRejectedWithoutEnergy},
        {"sleep rejected when already sleeping", sleepRejectedWhenAlreadySleeping},
        {"wake rejected when already awake", wakeRejectedWhenAlreadyAwake},
        {"apply dispatches care actions", applyDispatchesCareActions},
        {"xorShift32 known sequence", xorShift32KnownSequence},
        {"xorShift32 zero seed is non-zero", xorShift32ZeroSeedIsNonZeroStream},
        {"xorShift32 same seed same sequence", xorShift32SameSeedSameSequence},
        {"fakeRandom plays scripted values", fakeRandomPlaysScriptedValues},
        {"fakeRandom exhaustion fails", fakeRandomExhaustionFails},
        {"nextBounded uses queued sample", nextBoundedUsesQueuedSample},
        {"nextBounded zero does not consume", nextBoundedZeroDoesNotConsume},
        {"constructor does not consume rng", constructorDoesNotConsumeRng},
        {"sleeping update does not consume rng", sleepingUpdateDoesNotConsumeRng},
        {"pet state does not cache mood", petStateDoesNotCacheMood},
        {"deriveMood follows priority table", deriveMoodFollowsPriorityTable},
        {"mood is derived from snapshot", moodIsDerivedFromSnapshot},
        {"restore resets transient activity", restoreResetsTransientActivity},
        {"restore sleeping starts sleep activity", restoreSleepingStartsSleepActivity},
        {"idle rng 0 gives min duration", idleRngZeroGivesMinDuration},
        {"idle leftover feeds next decide timer", idleLeftoverFeedsNextDecideTimer},
        {"idle rng selects max duration and glance", idleRngSelectsMaxDurationAndGlance},
        {"tired idle adds duration bonus", tiredIdleAddsDurationBonus},
        {"egg freezes idle decide timer", eggFreezesIdleDecideTimer},
        {"sleep freezes idle decide timer", sleepFreezesIdleDecideTimer},
        {"wake resumes idle without rng", wakeResumesIdleWithoutRng},
        {"step size does not change autonomy", stepSizeDoesNotChangeAutonomy},
        {"catch-up remainder survives transition cap", catchUpRemainderSurvivesTransitionCap},
        {"pollEvent consumes once", pollEventConsumesOnce},
        {"game event queue is fixed and drops oldest", gameEventQueueIsFixedAndDropsOldest},
        {"pet event buffer does not grow past capacity", petEventBufferDoesNotGrowPastCapacity},
        {"sleep and wake emit uncompleted interrupt", sleepWakeEmitUncompletedInterrupt},
        {"rejected care does not emit events", rejectedCareDoesNotEmitEvents},
        {"frozen idle does not emit events", frozenIdleDoesNotEmitEvents},
        {"restore clears queued events", restoreClearsQueuedEvents},
        {"walk uses scripted facing and distance", walkUsesScriptedFacingAndDistance},
        {"walk advances x deterministically", walkAdvancesXDeterministically},
        {"walk completes to idle without wander roll", walkCompletesToIdleWithoutWanderRoll},
        {"walk step size does not change position", walkStepSizeDoesNotChangePosition},
        {"egg does not walk", eggDoesNotWalk},
        {"feed applied on walk starts eat", feedAppliedOnWalkStartsEat},
        {"eat completes to idle", eatCompletesToIdle},
        {"train and clean applied start happy", trainAndCleanAppliedStartHappy},
        {"rejected no energy does not restart tired", rejectedNoEnergyDoesNotRestartTired},
        {"rejected already awake keeps idle", rejectedAlreadyAwakeKeepsIdle},
        {"hygiene flank starts dirty once", hygieneFlankStartsDirtyOnce},
        {"annoyed flank starts annoyed", annoyedFlankStartsAnnoyed},
        {"restore does not fire mood one-shot", restoreDoesNotFireMoodOneShot},
        {"nap starts when energy critical", napStartsWhenEnergyCritical},
        {"nap auto wakes to idle", napAutoWakesToIdle},
        {"nap does not wake while energy low", napDoesNotWakeWhileEnergyLow},
        {"feed rejected during nap keeps nap", feedRejectedDuringNapKeepsNap},
        {"player sleep blocks nap and walk", playerSleepBlocksNapAndWalk},
        {"player wake ends nap", playerWakeEndsNap},
        {"happy idle uses bounce pool", happyIdleUsesBouncePool},
    };

    int failures = 0;
    for (const auto& test : tests) {
        bool passed = false;
        try {
            passed = test.run();
        } catch (const std::exception& ex) {
            std::cout << "[FAIL] " << test.name << " (" << ex.what() << ")\n";
            failures += 1;
            continue;
        }
        std::cout << (passed ? "[PASS] " : "[FAIL] ") << test.name << '\n';
        failures += passed ? 0 : 1;
    }
    std::cout << tests.size() - failures << '/' << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
