#include "FakeClock.hpp"
#include "FakeRandom.hpp"
#include "neripal/core/Activity.hpp"
#include "neripal/core/Autonomy.hpp"
#include "neripal/core/Balance.hpp"
#include "neripal/core/Care.hpp"
#include "neripal/core/EvolutionRules.hpp"
#include "neripal/core/GameEvent.hpp"
#include "neripal/core/Mood.hpp"
#include "neripal/core/Needs.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/core/PetSnapshot.hpp"
#include "neripal/core/SleepCause.hpp"
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
using neripal::core::PetSnapshot;
using neripal::core::PetState;
using neripal::core::SleepCause;
using neripal::core::XorShift32;

struct TestCase { std::string_view name; std::function<bool()> run; };

bool sameCareStats(const PetState& a, const PetState& b) {
    return a.hunger == b.hunger && a.happiness == b.happiness && a.energy == b.energy &&
           a.health == b.health && a.hygiene == b.hygiene && a.affection == b.affection &&
           a.stimulation == b.stimulation && a.sleeping == b.sleeping;
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

// Existing care tests assume a creature that has already left the egg.
template <typename Random>
Pet hatchedPet(FakeClock& clock, Random& rng) {
    Pet pet(clock, rng);
    auto state = pet.state();
    state.stage = neripal::core::EvolutionStage::Baby;
    state.form = neripal::core::FormId::Juvenile;
    pet.restore(state);
    return pet;
}

bool feedReducesHunger() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng); const int before = pet.state().hunger;
    return pet.feed() == CareResult::Applied && pet.state().hunger < before;
}
bool hungerNeverBelowZero() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng); for (int i = 0; i < 20; ++i) pet.feed();
    return pet.state().hunger == 0;
}
bool trainingConsumesEnergy() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng); const int before = pet.state().energy;
    return pet.train() == CareResult::Applied && pet.state().energy < before;
}
bool sleepingRecoversEnergy() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng); auto state = pet.state(); state.energy = 50;
    pet.restore(state); pet.sleep(); clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update(); return pet.state().energy > 50;
}
bool energyNeverAbove100() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng); auto state = pet.state(); state.energy = 99;
    pet.restore(state); pet.sleep(); clock.advance(neripal::core::balance::kNeedsStepMs * 10);
    pet.update(); return pet.state().energy == 100;
}
bool happinessIsClamped() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng); auto state = pet.state(); state.happiness = 999;
    pet.restore(state); if (pet.state().happiness != 100) return false;
    state.happiness = -50; pet.restore(state); return pet.state().happiness == 0;
}
bool healthIsClamped() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng); auto state = pet.state(); state.health = -1;
    pet.restore(state); if (pet.state().health != 0) return false;
    state.health = 101; pet.restore(state); return pet.state().health == 100;
}
bool controlledTimeIncreasesHunger() {
    FakeClock clock; XorShift32 rng(1u); Pet pet = hatchedPet(clock, rng); const int before = pet.state().hunger;
    clock.advance(neripal::core::balance::kNeedsStepMs); pet.update();
    return pet.state().hunger == before + neripal::core::balance::kHungerPerStep;
}
bool controlledTimeChangesEnergyByState() {
    FakeClock clock; XorShift32 rng(1u); Pet pet = hatchedPet(clock, rng); const int awake = pet.state().energy;
    clock.advance(neripal::core::balance::kNeedsStepMs); pet.update();
    if (pet.state().energy >= awake) return false;
    const int beforeSleep = pet.state().energy; pet.sleep();
    clock.advance(neripal::core::balance::kNeedsStepMs); pet.update();
    return pet.state().energy > beforeSleep;
}
bool noRealWaitIsNeeded() {
    FakeClock clock; XorShift32 rng(1u); Pet pet = hatchedPet(clock, rng);
    clock.advance(3 * neripal::core::balance::kNeedsStepMs); pet.update();
    return pet.state().ageMillis == 3 * neripal::core::balance::kNeedsStepMs;
}
bool restoreNormalizesEveryStat() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng);
    PetState invalid{};
    invalid.hunger = -5;
    invalid.happiness = 105;
    invalid.energy = -50;
    invalid.health = 800;
    invalid.hygiene = 200;
    invalid.affection = -4;
    invalid.stimulation = 140;
    invalid.ageMillis = 123;
    invalid.sleeping = true;
    pet.restore(invalid);
    const auto& state = pet.state();
    return state.hunger == 0 && state.happiness == 100 && state.energy == 0 &&
           state.health == 100 && state.hygiene == 100 && state.affection == 0 &&
           state.stimulation == 100 && state.ageMillis == 123 &&
           state.sleeping;
}
bool hygieneRestoreClampsLow() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng);
    PetState invalid = pet.state();
    invalid.hygiene = -8;
    pet.restore(invalid);
    return pet.state().hygiene == 0;
}
bool evolutionUsesControlledAge() {
    FakeClock clock; XorShift32 rng(1u); Pet pet = hatchedPet(clock, rng);
    clock.advance(neripal::core::evolution::kChildAgeMs); pet.update();
    if (pet.state().stage != neripal::core::EvolutionStage::Child) return false;
    clock.advance(neripal::core::evolution::kAdultAgeMs -
                  neripal::core::evolution::kChildAgeMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Adult;
}
bool eggHatchesWithControlledAge() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng); auto state = pet.state();
    state.stage = neripal::core::EvolutionStage::Egg;
    state.ageMillis = 0;
    pet.restore(state);
    if (pet.state().stage != neripal::core::EvolutionStage::Egg) return false;
    clock.advance(neripal::core::evolution::kEggHatchAgeMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Baby;
}
bool cleanRaisesHygiene() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng);
    auto state = pet.state();
    state.hygiene = 40;
    pet.restore(state);
    return pet.clean() == CareResult::Applied &&
           pet.state().hygiene == 40 + neripal::core::balance::kCleanHygiene;
}
bool cleanDoesNotExceedMax() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng);
    auto state = pet.state();
    state.hygiene = 90;
    pet.restore(state);
    return pet.clean() == CareResult::Applied && pet.state().hygiene == 100;
}
bool awakeTimeLowersHygiene() {
    FakeClock clock; XorShift32 rng(1u); Pet pet = hatchedPet(clock, rng);
    const int before = pet.state().hygiene;
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().hygiene == before + neripal::core::balance::kHygienePerAwakeStep;
}
bool sleepDoesNotLowerHygiene() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng);
    auto state = pet.state();
    state.hygiene = 50;
    pet.restore(state);
    pet.sleep();
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().hygiene == 50;
}
bool highHungerLowersHappinessOnlyWhenUrgent() {
    namespace B = neripal::core::balance;
    FakeClock clock; XorShift32 rng(1u); Pet pet = hatchedPet(clock, rng);
    auto state = pet.state();
    state.hunger = B::kHungerAttention;
    state.hygiene = 80;
    state.energy = 80;
    state.affection = 80;
    state.stimulation = 80;
    state.happiness = 50;
    pet.restore(state);
    clock.advance(B::kNeedsStepMs);
    pet.update();
    if (pet.state().hunger != B::kHungerAttention + 1 || pet.state().happiness != 50) return false;

    state = pet.state();
    state.hunger = B::kHungerUrgent - 1;
    state.happiness = 50;
    pet.restore(state);
    clock.advance(B::kNeedsStepMs);
    pet.update();
    return pet.state().hunger == B::kHungerUrgent && pet.state().happiness == 49;
}
bool lowHygieneLowersHappinessOnlyWhenUrgent() {
    namespace B = neripal::core::balance;
    FakeClock clock; XorShift32 rng(1u); Pet pet = hatchedPet(clock, rng);
    auto state = pet.state();
    state.hunger = 10;
    state.energy = 80;
    state.affection = 80;
    state.stimulation = 80;
    state.hygiene = B::kLowNeedAttention;
    state.happiness = 50;
    pet.restore(state);
    clock.advance(B::kNeedsStepMs);
    pet.update();
    if (pet.state().hygiene != B::kLowNeedAttention - 1 || pet.state().happiness != 50) return false;

    state = pet.state();
    state.hygiene = B::kLowNeedUrgent;
    state.happiness = 50;
    pet.restore(state);
    clock.advance(B::kNeedsStepMs);
    pet.update();
    return pet.state().hygiene == B::kLowNeedUrgent - 1 && pet.state().happiness == 49;
}
bool zeroHygieneLowersHealth() {
    FakeClock clock; XorShift32 rng(1u); Pet pet = hatchedPet(clock, rng);
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
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng);
    auto state = pet.state();
    state.sleeping = true;
    pet.restore(state);
    const auto before = pet.state();
    return pet.feed() == CareResult::RejectedAsleep && sameCareStats(before, pet.state()) &&
           sameAutonomySnapshot(before, pet.state());
}
bool trainRejectedWhenAsleep() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng);
    auto state = pet.state();
    state.sleeping = true;
    state.energy = 5;
    pet.restore(state);
    const auto before = pet.state();
    return pet.train() == CareResult::RejectedAsleep && sameCareStats(before, pet.state());
}
bool cleanRejectedWhenAsleep() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng);
    auto state = pet.state();
    state.sleeping = true;
    pet.restore(state);
    const auto before = pet.state();
    return pet.clean() == CareResult::RejectedAsleep && sameCareStats(before, pet.state());
}
bool trainRejectedWithoutEnergy() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng);
    auto state = pet.state();
    state.energy = -neripal::core::balance::kTrainEnergy - 1;
    pet.restore(state);
    const auto before = pet.state();
    return pet.train() == CareResult::RejectedNoEnergy && sameCareStats(before, pet.state()) &&
           pet.state().activity == Activity::Tired &&
           pet.state().activityDurationMs == neripal::core::balance::kTiredDurationMs;
}
bool sleepRejectedWhenAlreadySleeping() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng);
    pet.sleep();
    const auto before = pet.state();
    return pet.sleep() == CareResult::RejectedAlreadySleeping &&
           sameCareStats(before, pet.state()) && sameAutonomySnapshot(before, pet.state());
}
bool wakeRejectedWhenAlreadyAwake() {
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng);
    const auto before = pet.state();
    return pet.wake() == CareResult::RejectedAlreadyAwake && sameCareStats(before, pet.state()) &&
           sameAutonomySnapshot(before, pet.state());
}
bool applyDispatchesCareActions() {
    using neripal::core::CareAction;
    FakeClock clock; FakeRandom rng; Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
    if (pet.sleep() != CareResult::Applied) return false;
    clock.advance(10 * neripal::core::balance::kIdleDurationMinMs);
    pet.update();
    return pet.state().activity == Activity::Sleep && pet.state().activityElapsedMs == 0 &&
           pet.state().activityDurationMs == 0 && rng.remaining() == 4;
}

bool wakeResumesIdleWithoutRng() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);

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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
    auto egg = pet.state();
    egg.stage = neripal::core::EvolutionStage::Egg;
    egg.ageMillis = 0;
    pet.restore(egg);
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    GameEvent event{};
    if (pet.pollEvent(event)) return false;

    pet.reset();
    auto born = pet.state();
    born.stage = neripal::core::EvolutionStage::Baby;
    pet.restore(born);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
    if (pet.feed() != CareResult::Applied || pet.state().activity != Activity::Eat) return false;
    clock.advance(B::kEatDurationMs);
    pet.update();
    return pet.state().activity == Activity::Idle && pet.state().activityElapsedMs == 0;
}

bool trainAndCleanAppliedStartHappy() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    if (pet.train() != CareResult::Applied || pet.state().activity != Activity::Happy) {
        return false;
    }
    pet.reset();
    auto dirty = pet.state();
    dirty.hygiene = 20;
    dirty.stage = neripal::core::EvolutionStage::Baby;
    pet.restore(dirty);
    return pet.clean() == CareResult::Applied && pet.state().activity == Activity::Happy &&
           pet.state().activityDurationMs == B::kHappyDurationMs;
}

bool rejectedNoEnergyDoesNotRestartTired() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
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
    Pet pet = hatchedPet(clock, rng);
    auto happy = pet.state();
    happy.happiness = B::kHappyMoodHappiness + 1;
    pet.restore(happy);
    clock.advance(B::kIdleDurationMinMs);
    pet.update();
    return pet.state().idleVariant == IdleVariant::Bounce;
}

bool captureRestorePlayerSleep() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    if (pet.sleep() != CareResult::Applied) return false;
    const auto snap = pet.capture();
    if (snap.sleepCause != SleepCause::Player || snap.napRemainingMs != 0) return false;
    pet.reset();
    pet.restoreSnapshot(snap);
    return pet.state().sleeping && pet.state().activity == Activity::Sleep &&
           pet.capture().sleepCause == SleepCause::Player &&
           pet.state().activityDurationMs == 0;
}

bool captureRestoreNapKeepsRemaining() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    PetSnapshot snap = pet.capture();
    snap.sleepCause = SleepCause::Nap;
    snap.napRemainingMs = 12'000;
    snap.energy = B::kAutonomousNapEnergy;
    pet.restoreSnapshot(snap);
    const auto again = pet.capture();
    return pet.state().sleeping && pet.state().activity == Activity::Nap &&
           again.sleepCause == SleepCause::Nap && again.napRemainingMs == 12'000 &&
           pet.state().activityDurationMs == 12'000 && pet.state().activityElapsedMs == 0;
}

bool captureDoesNotPersistWalkPose() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    PetState walk = pet.state();
    walk.activity = Activity::Walk;
    walk.x = 40;
    walk.facing = -1;
    pet.restore(walk);
    const auto snap = pet.capture();
    pet.restoreSnapshot(snap);
    return snap.sleepCause == SleepCause::None && snap.napRemainingMs == 0 &&
           pet.state().x == B::kPetHomeX && pet.state().activity == Activity::Idle &&
           pet.state().facing == B::kDefaultFacing;
}

bool restoreSnapshotPreservesNeedsRemainder() {
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    clock.advance(30'000);
    pet.update();
    const auto snap = pet.capture();
    if (snap.needsRemainderMs != 30'000) return false;
    pet.reset();
    pet.restoreSnapshot(snap);
    return pet.capture().needsRemainderMs == 30'000;
}

bool restoreSnapshotZeroNapRemainingWakes() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    PetSnapshot snap = pet.capture();
    snap.sleepCause = SleepCause::Nap;
    snap.napRemainingMs = 0;
    pet.restoreSnapshot(snap);
    return !pet.state().sleeping && pet.capture().sleepCause == SleepCause::None &&
           pet.state().activity == Activity::Idle;
}

bool restoreSnapshotOversizedNapWakes() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    PetSnapshot snap = pet.capture();
    snap.sleepCause = SleepCause::Nap;
    snap.napRemainingMs = B::kNapDurationMaxMs + 1;
    pet.restoreSnapshot(snap);
    return !pet.state().sleeping && pet.capture().sleepCause == SleepCause::None;
}

bool restoreSnapshotDoesNotConsumeRng() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    PetSnapshot snap = pet.capture();
    snap.sleepCause = SleepCause::Nap;
    snap.napRemainingMs = 8'000;
    pet.restoreSnapshot(snap);
    return rng.remaining() == 0;
}

bool restoreSnapshotClearsQueuedEvents() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    pet.sleep();
    GameEvent event{};
    if (!pet.pollEvent(event)) return false;
    const auto snap = pet.capture();
    pet.wake();
    if (!pet.pollEvent(event)) return false;
    pet.restoreSnapshot(snap);
    return !pet.pollEvent(event);
}

// Live contract: energy already at kNapWakeEnergy ends the nap on the next
// tick even if napRemainingMs is still full. applyOffline uses the same rule.
bool restoredNapEndsWhenEnergyAtWakeThreshold() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({kSkipWalk, 0u, 0u});
    Pet pet = hatchedPet(clock, rng);
    PetSnapshot snap = pet.capture();
    snap.sleepCause = SleepCause::Nap;
    snap.napRemainingMs = B::kNapDurationMaxMs;
    snap.energy = B::kNapWakeEnergy;
    pet.restoreSnapshot(snap);
    if (pet.state().activity != Activity::Nap || pet.capture().napRemainingMs != B::kNapDurationMaxMs) {
        return false;
    }
    clock.advance(1);
    pet.update();
    return !pet.state().sleeping && pet.state().activity == Activity::Idle &&
           pet.capture().sleepCause == SleepCause::None && rng.remaining() == 0;
}

constexpr std::uint64_t kDayMs = 24ull * 60 * 60 * 1000;

bool sameNeeds(const PetSnapshot& a, const PetSnapshot& b) {
    return a.hunger == b.hunger && a.happiness == b.happiness && a.energy == b.energy &&
           a.health == b.health && a.hygiene == b.hygiene &&
           a.needsRemainderMs == b.needsRemainderMs;
}

int drainEvents(Pet& pet) {
    int count = 0;
    GameEvent event{};
    while (pet.pollEvent(event)) {
        ++count;
    }
    return count;
}

bool offlineZeroDoesNotChangeNeeds() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    const auto before = pet.capture();
    pet.applyOffline(0, 0);
    const auto after = pet.capture();
    return after.ageMillis == before.ageMillis && sameNeeds(before, after) &&
           after.sleepCause == SleepCause::None && pet.state().activity == Activity::Idle;
}

bool offlineThirtySecondsKeepsRemainder() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    const int hunger = pet.state().hunger;
    pet.applyOffline(30'000, 30'000);
    const auto snap = pet.capture();
    return snap.ageMillis == 30'000 && snap.needsRemainderMs == 30'000 && snap.hunger == hunger &&
           drainEvents(pet) <= 1;
}

bool offlineNinetySecondsAppliesOneNeedStep() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    const int hunger = pet.state().hunger;
    pet.applyOffline(90'000, 90'000);
    const auto snap = pet.capture();
    pet.update();
    return snap.ageMillis == 90'000 && snap.needsRemainderMs == 30'000 &&
           snap.hunger == hunger + 1 && pet.capture().ageMillis == 90'000;
}

bool offlineUsesExistingRemainder() {
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    clock.advance(40'000);
    pet.update();
    const int hunger = pet.state().hunger;
    pet.applyOffline(30'000, 30'000);
    const auto snap = pet.capture();
    return snap.ageMillis == 70'000 && snap.needsRemainderMs == 10'000 && snap.hunger == hunger + 1;
}

bool offlinePlayerSleepStaysAsleep() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    if (pet.sleep() != CareResult::Applied) return false;
    const int hygiene = pet.state().hygiene;
    const int energy = pet.state().energy;
    pet.applyOffline(120'000, 120'000);
    const auto snap = pet.capture();
    return snap.sleepCause == SleepCause::Player && pet.state().sleeping &&
           pet.state().activity == Activity::Sleep && snap.hygiene == hygiene &&
           snap.energy == energy + 4 && snap.napRemainingMs == 0 &&
           pet.wake() == CareResult::Applied;
}

bool offlineNapContinues() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    PetSnapshot snap = pet.capture();
    snap.sleepCause = SleepCause::Nap;
    snap.napRemainingMs = 15'000;
    snap.energy = B::kAutonomousNapEnergy;
    pet.restoreSnapshot(snap);
    pet.applyOffline(4'000, 4'000);
    const auto after = pet.capture();
    return after.sleepCause == SleepCause::Nap && after.napRemainingMs == 11'000 &&
           pet.state().activity == Activity::Nap && pet.state().sleeping &&
           after.energy == B::kAutonomousNapEnergy && rng.remaining() == 0;
}

bool offlineNapEndsByTimeThenAwake() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    PetSnapshot snap = pet.capture();
    snap.sleepCause = SleepCause::Nap;
    snap.napRemainingMs = 10'000;
    snap.energy = 80;
    snap.hygiene = 80;
    snap.needsRemainderMs = 0;
    pet.restoreSnapshot(snap);
    pet.applyOffline(70'000, 70'000);
    const auto after = pet.capture();
    return after.sleepCause == SleepCause::None && !pet.state().sleeping &&
           pet.state().activity == Activity::Idle && pet.state().x == neripal::core::balance::kPetHomeX &&
           after.energy == 79 && after.hygiene == 79 && after.needsRemainderMs == 10'000 &&
           rng.remaining() == 0;
}

bool offlineNapEndsEarlyWhenEnergyReachesWake() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    PetSnapshot snap = pet.capture();
    snap.sleepCause = SleepCause::Nap;
    snap.napRemainingMs = 20'000;
    snap.energy = B::kNapWakeEnergy;
    snap.hygiene = 80;
    snap.needsRemainderMs = 50'000;
    pet.restoreSnapshot(snap);
    pet.applyOffline(20'000, 20'000);
    const auto after = pet.capture();
    return after.sleepCause == SleepCause::None && pet.state().activity == Activity::Idle &&
           after.energy == B::kNapWakeEnergy - 1 && after.hygiene == 79 &&
           after.napRemainingMs == 0 && rng.remaining() == 0;
}

bool offlineFortyFiveDaysCapsNeedsOnly() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom fastRng;
    FakeRandom cappedRng;
    Pet fast = hatchedPet(clock, fastRng);
    Pet capped = hatchedPet(clock, cappedRng);
    fast.applyOffline(45ull * kDayMs, 45ull * kDayMs + 30'000);
    capped.applyOffline(0, B::kMaxNeedsOfflineMs);
    return fast.capture().ageMillis == 45ull * kDayMs &&
           fast.capture().needsRemainderMs == 0 && sameNeeds(fast.capture(), capped.capture());
}

bool offlineFourHundredDaysCapsAgeAndNeeds() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom fastRng;
    FakeRandom cappedRng;
    Pet fast = hatchedPet(clock, fastRng);
    Pet capped = hatchedPet(clock, cappedRng);
    fast.applyOffline(400ull * kDayMs, 400ull * kDayMs + 30'000);
    capped.applyOffline(0, B::kMaxNeedsOfflineMs);
    return fast.capture().ageMillis == B::kMaxAgeOfflineMs &&
           fast.capture().needsRemainderMs == 0 && sameNeeds(fast.capture(), capped.capture());
}

bool offlineDoesNotConsumeRngOrWalk() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng({0u, 0u, 0u, 7u, 8u, 9u});
    Pet pet = hatchedPet(clock, rng);
    clock.advance(B::kIdleDurationMinMs + B::kWalkMsPerPixel * 4);
    pet.update();
    if (pet.state().activity != Activity::Walk || pet.state().x == B::kPetHomeX) return false;
    const auto left = rng.remaining();
    pet.applyOffline(2ull * 60 * 60 * 1000, 2ull * 60 * 60 * 1000);
    return rng.remaining() == left && pet.state().activity != Activity::Walk &&
           pet.state().x == B::kPetHomeX && drainEvents(pet) <= 1;
}

bool offlineSettleMatchesMinuteOracle() {
    FakeClock clock;
    FakeRandom fastRng;
    FakeRandom slowRng;
    PetSnapshot snap;
    snap.hunger = 0;
    snap.happiness = 100;
    snap.energy = 100;
    snap.health = 100;
    snap.hygiene = 100;
    snap.needsRemainderMs = 12'345;
    snap.sleepCause = SleepCause::None;
    snap.stage = neripal::core::EvolutionStage::Baby;

    Pet fast = hatchedPet(clock, fastRng);
    fast.restoreSnapshot(snap);
    fast.applyOffline(0, 30ull * kDayMs);

    Pet slow = hatchedPet(clock, slowRng);
    slow.restoreSnapshot(snap);
    const std::uint64_t minutes = 30ull * 24 * 60;
    for (std::uint64_t i = 0; i < minutes; ++i) {
        slow.applyOffline(0, 60'000);
    }
    return sameNeeds(fast.capture(), slow.capture()) &&
           fast.capture().sleepCause == slow.capture().sleepCause &&
           fast.state().activity == Activity::Idle && fastRng.remaining() == 0 &&
           slowRng.remaining() == 0;
}

bool offlinePlayerSleepSettleMatchesMinuteOracle() {
    FakeClock clock;
    FakeRandom fastRng;
    FakeRandom slowRng;
    PetSnapshot snap;
    snap.hunger = 0;
    snap.happiness = 100;
    snap.energy = 0;
    snap.health = 100;
    snap.hygiene = 80;
    snap.needsRemainderMs = 1'000;
    snap.sleepCause = SleepCause::Player;
    snap.stage = neripal::core::EvolutionStage::Baby;

    Pet fast = hatchedPet(clock, fastRng);
    fast.restoreSnapshot(snap);
    fast.applyOffline(0, 1'000ull * 60'000);

    Pet slow = hatchedPet(clock, slowRng);
    slow.restoreSnapshot(snap);
    for (int i = 0; i < 1000; ++i) {
        slow.applyOffline(0, 60'000);
    }
    return sameNeeds(fast.capture(), slow.capture()) &&
           fast.capture().sleepCause == SleepCause::Player &&
           slow.capture().sleepCause == SleepCause::Player &&
           fast.state().activity == Activity::Sleep;
}

bool needLevelsFollowProvisionalThresholds() {
    using neripal::core::Need;
    using neripal::core::NeedLevel;
    using neripal::core::needLevel;
    namespace B = neripal::core::balance;

    if (needLevel(Need::Hunger, B::kHungerAttention - 1) != NeedLevel::Normal) return false;
    if (needLevel(Need::Hunger, B::kHungerAttention) != NeedLevel::Attention) return false;
    if (needLevel(Need::Hunger, B::kHungerUrgent - 1) != NeedLevel::Attention) return false;
    if (needLevel(Need::Hunger, B::kHungerUrgent) != NeedLevel::Urgent) return false;

    const Need lows[] = {Need::Energy, Need::Hygiene, Need::Affection, Need::Stimulation};
    for (const Need need : lows) {
        if (needLevel(need, B::kLowNeedAttention + 1) != NeedLevel::Normal) return false;
        if (needLevel(need, B::kLowNeedAttention) != NeedLevel::Attention) return false;
        if (needLevel(need, B::kLowNeedUrgent + 1) != NeedLevel::Attention) return false;
        if (needLevel(need, B::kLowNeedUrgent) != NeedLevel::Urgent) return false;
    }
    return true;
}

bool oneUrgentDropsHappinessOnce() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    auto state = pet.state();
    state.hunger = B::kHungerUrgent;
    state.hygiene = B::kLowNeedUrgent;
    state.energy = 80;
    state.affection = 80;
    state.stimulation = 80;
    state.happiness = 50;
    state.health = 80;
    pet.restore(state);
    clock.advance(B::kNeedsStepMs);
    pet.update();
    return pet.state().happiness == 49 && pet.state().health == 80;
}

bool affectionUrgentDoesNotLowerHealth() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    auto state = pet.state();
    state.affection = 0;
    state.hunger = 10;
    state.energy = 80;
    state.hygiene = 80;
    state.stimulation = 80;
    state.health = 50;
    state.happiness = 50;
    pet.restore(state);
    clock.advance(B::kNeedsStepMs);
    pet.update();
    return pet.state().health == 50 && pet.state().happiness == 49 && pet.state().affection == 0;
}

bool awakeAffectionAndStimulationFollowCadence() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    const int affection = pet.state().affection;
    const int stimulation = pet.state().stimulation;
    clock.advance(2 * B::kNeedsStepMs);
    pet.update();
    if (pet.state().affection != affection || pet.state().stimulation != stimulation - 2) return false;
    clock.advance(B::kNeedsStepMs);
    pet.update();
    return pet.state().affection == affection - 1 && pet.state().stimulation == stimulation - 3;
}

bool sleepSlowsHygieneAndAffectionAndHoldsStimulation() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    const int hygiene = pet.state().hygiene;
    const int affection = pet.state().affection;
    const int stimulation = pet.state().stimulation;
    if (pet.sleep() != CareResult::Applied) return false;
    clock.advance(3 * B::kNeedsStepMs);
    pet.update();
    if (pet.state().hygiene != hygiene || pet.state().affection != affection ||
        pet.state().stimulation != stimulation) {
        return false;
    }
    clock.advance(B::kNeedsStepMs);
    pet.update();
    if (pet.state().hygiene != hygiene - 1 || pet.state().affection != affection) return false;
    clock.advance(2 * B::kNeedsStepMs);
    pet.update();
    return pet.state().hygiene == hygiene - 1 && pet.state().affection == affection - 1 &&
           pet.state().stimulation == stimulation;
}

bool petRaisesAffectionWithoutStimulation() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    const int stimulation = pet.state().stimulation;
    const int happiness = pet.state().happiness;
    return pet.pet() == CareResult::Applied &&
           pet.state().affection == B::kAffectionStart + B::kPetAffection &&
           pet.state().stimulation == stimulation &&
           pet.state().happiness == happiness + B::kPetHappiness &&
           pet.state().activity == Activity::Happy;
}

bool playRaisesStimulationWithoutAffection() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    const int affection = pet.state().affection;
    const int energy = pet.state().energy;
    const int hunger = pet.state().hunger;
    const int happiness = pet.state().happiness;
    return pet.play() == CareResult::Applied &&
           pet.state().stimulation == B::kStimulationStart + B::kPlayStimulation &&
           pet.state().affection == affection &&
           pet.state().energy == energy + B::kPlayEnergy &&
           pet.state().hunger == hunger + B::kPlayHunger &&
           pet.state().happiness == happiness + B::kPlayHappiness;
}

bool trainRaisesStimulationWithoutAffection() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    auto state = pet.state();
    state.health = 90;
    pet.restore(state);
    return pet.train() == CareResult::Applied &&
           pet.state().stimulation == B::kStimulationStart + B::kTrainStimulation &&
           pet.state().affection == B::kAffectionStart &&
           pet.state().health == 92;
}

bool playRejectedWithoutEnergy() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    auto low = pet.state();
    low.energy = -B::kPlayEnergy - 1;
    pet.restore(low);
    const auto before = pet.state();
    return pet.play() == CareResult::RejectedNoEnergy && sameCareStats(before, pet.state()) &&
           pet.state().activity == Activity::Tired;
}

bool petRejectedWhenAsleep() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    if (pet.sleep() != CareResult::Applied) return false;
    const auto before = pet.state();
    return pet.pet() == CareResult::RejectedAsleep && sameCareStats(before, pet.state());
}

bool captureRestoresAffectionAndNeedsPhase() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    if (pet.pet() != CareResult::Applied) return false;
    clock.advance(3 * B::kNeedsStepMs);
    pet.update();
    const auto snap = pet.capture();
    if (snap.needsStepPhase != 3 || snap.affection != B::kAffectionStart + B::kPetAffection - 1) {
        return false;
    }
    pet.reset();
    pet.restoreSnapshot(snap);
    const auto again = pet.capture();
    return again.needsStepPhase == 3 && again.affection == snap.affection &&
           again.stimulation == snap.stimulation;
}

bool offlineNapSplitSettleMatchesMinuteOracle() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom fastRng;
    FakeRandom slowRng;
    PetSnapshot snap;
    snap.hunger = 10;
    snap.happiness = 90;
    snap.energy = 38;
    snap.health = 100;
    snap.hygiene = 90;
    snap.needsRemainderMs = 50'000;
    snap.sleepCause = SleepCause::Nap;
    snap.napRemainingMs = 15'000;
    snap.stage = neripal::core::EvolutionStage::Baby;

    const std::uint64_t elapsed = 800ull * 60'000;
    Pet fast = hatchedPet(clock, fastRng);
    fast.restoreSnapshot(snap);
    fast.applyOffline(0, elapsed);

    Pet slow = hatchedPet(clock, slowRng);
    slow.restoreSnapshot(snap);
    for (int i = 0; i < 800; ++i) {
        slow.applyOffline(0, 60'000);
    }
    return sameNeeds(fast.capture(), slow.capture()) &&
           fast.capture().sleepCause == SleepCause::None &&
           slow.capture().sleepCause == SleepCause::None &&
           fast.state().activity == Activity::Idle &&
           fast.state().energy < B::kNapWakeEnergy + 2;
}

PetState watchedHunger(int hunger) {
    PetState state;
    state.hunger = hunger;
    state.energy = 100;
    state.hygiene = 100;
    state.affection = 100;
    state.stimulation = 100;
    state.happiness = 80;
    state.health = 90;
    state.stage = neripal::core::EvolutionStage::Baby;
    return state;
}

void advanceMinutes(FakeClock& clock, Pet& pet, int minutes) {
    clock.advance(static_cast<std::uint64_t>(minutes) * neripal::core::balance::kNeedsStepMs);
    pet.update();
}

bool attentionDoesNotOpenCareEpisode() {
    namespace B = neripal::core::balance;
    using neripal::core::EpisodeState;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    pet.restore(watchedHunger(B::kHungerAttention));
    advanceMinutes(clock, pet, 1);
    const auto& episode = pet.careRecord().episodes[0];
    return episode.state == EpisodeState::None &&
           neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby)
                   .careMistakes == 0 &&
           pet.state().hunger == B::kHungerAttention + 1;
}

bool urgentOpensEpisodeWithoutMistake() {
    namespace B = neripal::core::balance;
    using neripal::core::EpisodeState;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    pet.restore(watchedHunger(B::kHungerUrgent));
    advanceMinutes(clock, pet, 1);
    const auto& episode = pet.careRecord().episodes[0];
    const auto& history =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby);
    return episode.state == EpisodeState::Open &&
           episode.stepsRemaining == B::kAttentionWindowSteps && history.careMistakes == 0 &&
           pet.careRecord().lifetimeCareMistakes == 0;
}

bool careBeforeTimeoutAddsNoMistake() {
    namespace B = neripal::core::balance;
    using neripal::core::EpisodeState;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    pet.restore(watchedHunger(B::kHungerUrgent));
    advanceMinutes(clock, pet, 1 + B::kAttentionWindowSteps - 1);
    if (pet.careRecord().episodes[0].stepsRemaining != 1) return false;
    if (pet.feed() != CareResult::Applied) return false;
    advanceMinutes(clock, pet, 1);
    const auto& history =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby);
    return pet.careRecord().episodes[0].state == EpisodeState::None && history.careMistakes == 0 &&
           pet.careRecord().lifetimeCareMistakes == 0;
}

bool careTimeoutAddsExactlyOneMistake() {
    namespace B = neripal::core::balance;
    using neripal::core::EpisodeState;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    pet.restore(watchedHunger(B::kHungerUrgent));
    advanceMinutes(clock, pet, 1 + B::kAttentionWindowSteps);
    const auto& episode = pet.careRecord().episodes[0];
    const auto& history =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby);
    return episode.state == EpisodeState::Counted && history.careMistakes == 1 &&
           pet.careRecord().lifetimeCareMistakes == 1 &&
           neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Child)
                   .careMistakes == 0;
}

bool stayingUrgentDoesNotAddAnotherMistake() {
    namespace B = neripal::core::balance;
    using neripal::core::EpisodeState;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    pet.restore(watchedHunger(B::kHungerUrgent));
    advanceMinutes(clock, pet, 1 + B::kAttentionWindowSteps + 30);
    const auto& history =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby);
    return pet.careRecord().episodes[0].state == EpisodeState::Counted &&
           history.careMistakes == 1 && pet.careRecord().lifetimeCareMistakes == 1;
}

bool leavingUrgentClosesEpisode() {
    namespace B = neripal::core::balance;
    using neripal::core::EpisodeState;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    pet.restore(watchedHunger(B::kHungerUrgent));
    advanceMinutes(clock, pet, 1);
    if (pet.careRecord().episodes[0].state != EpisodeState::Open) return false;
    if (pet.feed() != CareResult::Applied) return false;
    advanceMinutes(clock, pet, 1);
    return pet.careRecord().episodes[0].state == EpisodeState::None &&
           neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby)
                   .careMistakes == 0;
}

bool reenteringUrgentOpensNewEpisode() {
    namespace B = neripal::core::balance;
    using neripal::core::EpisodeState;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    pet.restore(watchedHunger(B::kHungerUrgent));
    advanceMinutes(clock, pet, 1 + B::kAttentionWindowSteps);
    if (pet.careRecord().lifetimeCareMistakes != 1) return false;
    auto cooled = pet.state();
    cooled.hunger = 10;
    pet.restore(cooled);
    advanceMinutes(clock, pet, 1);
    if (pet.careRecord().episodes[0].state != EpisodeState::None) return false;
    auto again = pet.state();
    again.hunger = B::kHungerUrgent;
    pet.restore(again);
    advanceMinutes(clock, pet, 1);
    return pet.careRecord().episodes[0].state == EpisodeState::Open &&
           pet.careRecord().lifetimeCareMistakes == 1;
}

bool sleepPausesAttentionWindow() {
    namespace B = neripal::core::balance;
    using neripal::core::EpisodeState;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    pet.restore(watchedHunger(B::kHungerUrgent));
    advanceMinutes(clock, pet, 1);
    if (pet.sleep() != CareResult::Applied) return false;
    advanceMinutes(clock, pet, 20);
    const auto& paused = pet.careRecord().episodes[0];
    if (paused.state != EpisodeState::Open || paused.stepsRemaining != B::kAttentionWindowSteps) {
        return false;
    }
    if (pet.wake() != CareResult::Applied) return false;
    if (pet.careRecord().episodes[0].stepsRemaining != B::kAttentionWindowSteps) return false;
    advanceMinutes(clock, pet, 1);
    return pet.careRecord().episodes[0].state == EpisodeState::Open &&
           pet.careRecord().episodes[0].stepsRemaining == B::kAttentionWindowSteps - 1 &&
           neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby)
                   .careMistakes == 0;
}

bool offlineDoesNotTouchWindowOrMistakes() {
    namespace B = neripal::core::balance;
    using neripal::core::EpisodeState;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    pet.restore(watchedHunger(B::kHungerUrgent));
    advanceMinutes(clock, pet, 1);
    const auto stepsBefore =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby).steps;
    pet.applyOffline(20 * B::kNeedsStepMs, 20 * B::kNeedsStepMs);
    const auto& episode = pet.careRecord().episodes[0];
    const auto& history =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby);
    if (episode.state != EpisodeState::Open || episode.stepsRemaining != B::kAttentionWindowSteps ||
        history.careMistakes != 0 || history.steps <= stepsBefore) {
        return false;
    }

    Pet untouched = hatchedPet(clock, rng);
    untouched.restore(watchedHunger(B::kHungerAttention));
    untouched.applyOffline(30 * B::kNeedsStepMs, 30 * B::kNeedsStepMs);
    return untouched.careRecord().episodes[0].state == EpisodeState::None &&
           untouched.careRecord().lifetimeCareMistakes == 0 &&
           untouched.state().hunger >= B::kHungerUrgent;
}

bool responseTimeCountsOpenMinutes() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    pet.restore(watchedHunger(B::kHungerUrgent));
    advanceMinutes(clock, pet, 1 + 4);
    if (pet.feed() != CareResult::Applied) return false;
    advanceMinutes(clock, pet, 1);
    const auto& history =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby);
    return history.responseCount == 1 && history.responseStepsSum == 4 && history.careMistakes == 0;
}

bool trainIncrementsTrainingCountAndPlayDoesNot() {
    namespace B = neripal::core::balance;
    using neripal::core::TrainingLevel;
    using neripal::core::trainingLevel;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    const auto& history =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby);
    if (pet.play() != CareResult::Applied || history.trainCount != 0) return false;
    auto empty = pet.state();
    empty.energy = 0;
    pet.restore(empty);
    if (pet.train() != CareResult::RejectedNoEnergy || history.trainCount != 0) return false;

    int applied = 0;
    while (applied < B::kTrainingFrequentCount) {
        if (pet.state().energy < -B::kTrainEnergy) {
            auto rested = pet.state();
            rested.energy = 100;
            pet.restore(rested);
        }
        if (pet.train() != CareResult::Applied) return false;
        ++applied;
        const auto level = trainingLevel(history);
        if (applied < B::kTrainingModerateCount && level != TrainingLevel::Low) return false;
        if (applied >= B::kTrainingModerateCount && applied < B::kTrainingFrequentCount &&
            level != TrainingLevel::Moderate) {
            return false;
        }
    }
    if (pet.play() != CareResult::Applied) return false;
    return history.trainCount == B::kTrainingFrequentCount &&
           trainingLevel(history) == TrainingLevel::Frequent;
}

bool stageHistoryIsKeptWhenAgeMovesStage() {
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    advanceMinutes(clock, pet, 1);
    const auto babySteps =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby).steps;
    if (babySteps != 1) return false;
    auto older = pet.state();
    older.ageMillis = neripal::core::evolution::kChildAgeMs;
    older.stage = neripal::core::EvolutionStage::Child;
    pet.restore(older);
    if (neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby).steps !=
        babySteps) {
        return false;
    }
    advanceMinutes(clock, pet, 1);
    return pet.state().stage == neripal::core::EvolutionStage::Child &&
           neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby).steps ==
               babySteps &&
           neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Child).steps ==
               1;
}

bool newGameStartsAsEgg() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    return pet.state().stage == neripal::core::EvolutionStage::Egg &&
           pet.feed() == CareResult::RejectedEgg;
}

bool eggDoesNotChangeBeforeHatch() {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    const auto before = pet.state();
    clock.advance(E::kEggHatchAgeMs - 1);
    pet.update();
    const auto after = pet.state();
    return after.stage == neripal::core::EvolutionStage::Egg &&
           after.ageMillis == E::kEggHatchAgeMs - 1 && sameCareStats(before, after);
}

bool eggHatchesAtThreshold() {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    const int hunger = pet.state().hunger;
    clock.advance(E::kEggHatchAgeMs);
    pet.update();
    GameEvent event{};
    if (pet.state().stage != neripal::core::EvolutionStage::Baby) return false;
    if (pet.state().form != neripal::core::FormId::Juvenile) return false;
    if (pet.state().hunger != hunger) return false;
    if (!pet.pollEvent(event) || event.kind != GameEventKind::Hatched) return false;
    return !pet.pollEvent(event);
}

bool babyBecomesChildAtAge() {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    clock.advance(E::kChildAgeMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Child &&
           pet.state().form == neripal::core::FormId::Juvenile &&
           pet.state().ageMillis == E::kChildAgeMs;
}

bool childBecomesAdultAtAge() {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    auto snap = pet.capture();
    snap.stage = neripal::core::EvolutionStage::Child;
    snap.ageMillis = E::kChildAgeMs;
    pet.restoreSnapshot(snap);
    clock.advance(E::kAdultAgeMs - E::kChildAgeMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Adult;
}

bool adultBecomesFinalAtAge() {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    auto snap = pet.capture();
    snap.stage = neripal::core::EvolutionStage::Adult;
    snap.ageMillis = E::kAdultAgeMs;
    pet.restoreSnapshot(snap);
    clock.advance(E::kFinalAgeMs - E::kAdultAgeMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Final;
}

bool longUpdateDoesNotSkipStages() {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    clock.advance(E::kAdultAgeMs);
    pet.update();
    const auto egg =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Egg).steps;
    const auto baby =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby).steps;
    const auto child =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Child).steps;
    const auto adult =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Adult).steps;
    return pet.state().stage == neripal::core::EvolutionStage::Adult && egg == 0 &&
           baby == 5u * 60u && child == 18u * 60u && adult == 0;
}

bool overdueAgeAdvancesOnlyOneStage() {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    auto snap = pet.capture();
    snap.stage = neripal::core::EvolutionStage::Egg;
    snap.ageMillis = E::kFinalAgeMs;
    pet.restoreSnapshot(snap);
    if (pet.state().stage != neripal::core::EvolutionStage::Egg) return false;
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Baby &&
           neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Child)
                   .steps == 0;
}

bool restoreKeepsPersistedStage() {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    auto adult = pet.state();
    adult.stage = neripal::core::EvolutionStage::Adult;
    adult.ageMillis = 0;
    pet.restore(adult);
    if (pet.state().stage != neripal::core::EvolutionStage::Adult) return false;
    auto egg = pet.state();
    egg.stage = neripal::core::EvolutionStage::Egg;
    egg.ageMillis = E::kFinalAgeMs;
    pet.restore(egg);
    return pet.state().stage == neripal::core::EvolutionStage::Egg &&
           pet.state().ageMillis == E::kFinalAgeMs;
}

bool restoreSnapshotKeepsPersistedStage() {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    auto snap = pet.capture();
    snap.stage = neripal::core::EvolutionStage::Final;
    snap.ageMillis = 0;
    pet.restoreSnapshot(snap);
    if (pet.state().stage != neripal::core::EvolutionStage::Final) return false;
    snap.stage = neripal::core::EvolutionStage::Egg;
    snap.ageMillis = E::kAdultAgeMs;
    pet.restoreSnapshot(snap);
    return pet.state().stage == neripal::core::EvolutionStage::Egg &&
           pet.state().ageMillis == E::kAdultAgeMs;
}

bool eggDoesNotDegradeStats() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    const auto before = pet.state();
    clock.advance(30 * neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Egg &&
           sameCareStats(before, pet.state());
}

bool eggDoesNotOpenEpisodesOrHistory() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    auto state = pet.state();
    state.hunger = 100;
    state.energy = 0;
    state.hygiene = 0;
    state.affection = 0;
    state.stimulation = 0;
    pet.restore(state);
    advanceMinutes(clock, pet, 20);
    if (pet.state().stage != neripal::core::EvolutionStage::Egg) return false;
    if (pet.state().hunger != 100 || pet.state().energy != 0 || pet.state().hygiene != 0) {
        return false;
    }
    const auto& care = pet.careRecord();
    if (care.lifetimeCareMistakes != 0) return false;
    if (neripal::core::historyFor(care, neripal::core::EvolutionStage::Egg).steps != 0) {
        return false;
    }
    for (const auto& episode : care.episodes) {
        if (episode.state != neripal::core::EpisodeState::None) return false;
    }
    return true;
}

bool careActionsRejectedOnEgg() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    const auto before = pet.state();
    const CareResult results[] = {
        pet.feed(), pet.train(), pet.sleep(), pet.wake(),
        pet.clean(), pet.pet(), pet.play(), pet.apply(neripal::core::CareAction::Feed),
    };
    for (const auto result : results) {
        if (result != CareResult::RejectedEgg) return false;
    }
    return sameCareStats(before, pet.state()) && !pet.state().sleeping;
}

bool stageChangeStartsNewHistoryAndKeepsPrevious() {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    advanceMinutes(clock, pet, 1);
    const auto babyBefore =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby).steps;
    if (babyBefore != 1) return false;
    const auto left = E::kChildAgeMs - pet.state().ageMillis;
    clock.advance(left);
    pet.update();
    if (pet.state().stage != neripal::core::EvolutionStage::Child) return false;
    const auto babyAtGate =
        neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby).steps;
    if (babyAtGate <= babyBefore) return false;
    if (neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Child).steps !=
        0) {
        return false;
    }
    advanceMinutes(clock, pet, 1);
    return pet.state().stage == neripal::core::EvolutionStage::Child &&
           neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby)
                   .steps == babyAtGate &&
           neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Child)
                   .steps == 1;
}

PetSnapshot childAtAdultGate(std::uint16_t mistakes, std::uint16_t trains, std::uint32_t steps,
                             std::uint32_t healthGood, std::uint32_t happinessGood) {
    PetSnapshot snap;
    snap.stage = neripal::core::EvolutionStage::Child;
    snap.form = neripal::core::FormId::Juvenile;
    snap.ageMillis = neripal::core::evolution::kAdultAgeMs;
    snap.sleepCause = SleepCause::Player;
    snap.energy = 80;
    auto& history =
        neripal::core::historyFor(snap.care, neripal::core::EvolutionStage::Child);
    history.careMistakes = mistakes;
    history.trainCount = trains;
    history.steps = steps;
    history.healthGoodSteps = healthGood;
    history.happinessGoodSteps = happinessGood;
    return snap;
}

bool evolveSleepingChild(Pet& pet, FakeClock& clock, const PetSnapshot& snap) {
    pet.restoreSnapshot(snap);
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Adult;
}

bool childToAdultFollowsFixtureRules() {
    FakeClock clock;
    FakeRandom rng;
    Pet frequent = hatchedPet(clock, rng);
    Pet balanced = hatchedPet(clock, rng);
    Pet neglected = hatchedPet(clock, rng);
    const auto frequentSnap = childAtAdultGate(0, 18, 100, 100, 100);
    const auto balancedSnap = childAtAdultGate(2, 0, 100, 0, 60);
    const auto neglectedSnap = childAtAdultGate(3, 0, 100, 100, 100);
    if (!evolveSleepingChild(frequent, clock, frequentSnap)) return false;
    if (!evolveSleepingChild(balanced, clock, balancedSnap)) return false;
    if (!evolveSleepingChild(neglected, clock, neglectedSnap)) return false;
    return frequent.state().form == neripal::core::FormId::AdultA &&
           balanced.state().form == neripal::core::FormId::AdultB &&
           neglected.state().form == neripal::core::FormId::AdultC && rng.remaining() == 0;
}

bool specialRulePicksSecretOrAdultB() {
    using neripal::core::EvolutionContext;
    using neripal::core::EvolutionStage;
    using neripal::core::FormId;
    using neripal::core::resolveEvolution;
    const auto snap = childAtAdultGate(0, 6, 100, 90, 90);
    EvolutionContext context;
    context.care = &snap.care;
    context.leaving = EvolutionStage::Child;
    FakeRandom secretRng(std::vector<std::uint32_t>{0u});
    FakeRandom branchRng(std::vector<std::uint32_t>{1u});
    return resolveEvolution(context, secretRng) == FormId::AdultSecret &&
           secretRng.remaining() == 0 &&
           resolveEvolution(context, branchRng) == FormId::AdultB && branchRng.remaining() == 0;
}

bool higherPriorityRuleWins() {
    using neripal::core::EvolutionContext;
    using neripal::core::EvolutionStage;
    using neripal::core::FormId;
    using neripal::core::resolveEvolution;
    const auto snap = childAtAdultGate(0, 6, 100, 100, 100);
    EvolutionContext context;
    context.care = &snap.care;
    context.leaving = EvolutionStage::Child;
    FakeRandom rng(std::vector<std::uint32_t>{0u});
    return resolveEvolution(context, rng) == FormId::AdultSecret;
}

bool fallbackResolvesEmptyHistory() {
    using neripal::core::EvolutionContext;
    using neripal::core::EvolutionStage;
    using neripal::core::FormId;
    using neripal::core::resolveEvolution;
    PetSnapshot snap;
    snap.stage = EvolutionStage::Child;
    EvolutionContext context;
    context.care = &snap.care;
    context.leaving = EvolutionStage::Child;
    FakeRandom rng;
    return resolveEvolution(context, rng) == FormId::AdultC;
}

bool identicalRaisingWithoutRngMatches() {
    using neripal::core::EvolutionContext;
    using neripal::core::EvolutionStage;
    using neripal::core::resolveEvolution;
    const auto first = childAtAdultGate(0, 18, 100, 40, 40);
    const auto second = childAtAdultGate(0, 18, 100, 40, 40);
    EvolutionContext left;
    left.care = &first.care;
    left.leaving = EvolutionStage::Child;
    EvolutionContext right;
    right.care = &second.care;
    right.leaving = EvolutionStage::Child;
    FakeRandom leftRng;
    FakeRandom rightRng;
    return resolveEvolution(left, leftRng) == resolveEvolution(right, rightRng);
}

bool evolutionRngConsumedOnce() {
    FakeClock clock;
    FakeRandom rng(std::vector<std::uint32_t>{0u, 7u});
    Pet pet = hatchedPet(clock, rng);
    const auto snap = childAtAdultGate(0, 6, 100, 100, 100);
    if (!evolveSleepingChild(pet, clock, snap)) return false;
    if (pet.state().form != neripal::core::FormId::AdultSecret || rng.remaining() != 1) {
        return false;
    }
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().form == neripal::core::FormId::AdultSecret &&
           pet.state().stage == neripal::core::EvolutionStage::Adult && rng.remaining() == 1;
}

bool laterTickKeepsForm() {
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    const auto snap = childAtAdultGate(0, 18, 100, 100, 100);
    if (!evolveSleepingChild(pet, clock, snap)) return false;
    const auto chosen = pet.state().form;
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().form == chosen && pet.state().form == neripal::core::FormId::AdultA &&
           pet.state().stage == neripal::core::EvolutionStage::Adult;
}

bool restoreSnapshotKeepsFormWithoutReroll() {
    FakeClock clock;
    FakeRandom rng(std::vector<std::uint32_t>{0u});
    Pet pet = hatchedPet(clock, rng);
    auto snap = childAtAdultGate(0, 18, 100, 100, 100);
    snap.stage = neripal::core::EvolutionStage::Adult;
    snap.form = neripal::core::FormId::AdultC;
    pet.restoreSnapshot(snap);
    if (pet.state().form != neripal::core::FormId::AdultC) return false;
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Adult &&
           pet.state().form == neripal::core::FormId::AdultC && rng.remaining() == 1;
}

bool adultToFinalKeepsForm() {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    auto snap = pet.capture();
    snap.stage = neripal::core::EvolutionStage::Adult;
    snap.form = neripal::core::FormId::AdultB;
    snap.ageMillis = E::kFinalAgeMs - 1;
    snap.sleepCause = SleepCause::Player;
    pet.restoreSnapshot(snap);
    clock.advance(1);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Final &&
           pet.state().form == neripal::core::FormId::AdultB && rng.remaining() == 0;
}

bool evolutionContextReadsOtherStageAndLifetime() {
    using neripal::core::EvolutionContext;
    using neripal::core::EvolutionRule;
    using neripal::core::EvolutionStage;
    using neripal::core::FormId;
    using neripal::core::ruleMatches;
    using neripal::core::resolveEvolution;
    PetSnapshot snap;
    auto& baby = neripal::core::historyFor(snap.care, EvolutionStage::Baby);
    baby.careMistakes = 4;
    baby.steps = 10;
    auto& child = neripal::core::historyFor(snap.care, EvolutionStage::Child);
    child.careMistakes = 0;
    child.trainCount = 18;
    child.steps = 10;
    neripal::core::historyFor(snap.care, EvolutionStage::Adult).steps = 3;
    neripal::core::historyFor(snap.care, EvolutionStage::Final).steps = 8;
    snap.care.lifetimeCareMistakes = 9;

    EvolutionContext context;
    context.care = &snap.care;
    context.leaving = EvolutionStage::Child;
    if (context.stageHistory(EvolutionStage::Baby).careMistakes != 4) return false;
    if (context.stageHistory(EvolutionStage::Adult).steps != 3) return false;
    if (context.stageHistory(EvolutionStage::Final).steps != 8) return false;
    if (context.lifetimeCareMistakes() != 9) return false;

    EvolutionRule babyMistakes{};
    babyMistakes.from = EvolutionStage::Child;
    babyMistakes.exits[0] = FormId::AdultC;
    babyMistakes.exitCount = 1;
    babyMistakes.mistakes.enabled = true;
    babyMistakes.mistakes.stage = EvolutionStage::Baby;
    babyMistakes.mistakes.exact = true;
    babyMistakes.mistakes.maximum = 4;
    if (!ruleMatches(babyMistakes, context)) return false;

    EvolutionRule childExact{};
    childExact.mistakes.enabled = true;
    childExact.mistakes.stage = EvolutionStage::Child;
    childExact.mistakes.exact = true;
    childExact.mistakes.maximum = 4;
    if (ruleMatches(childExact, context)) return false;

    EvolutionRule lifetimeExact{};
    lifetimeExact.mistakes.enabled = true;
    lifetimeExact.mistakes.lifetime = true;
    lifetimeExact.mistakes.exact = true;
    lifetimeExact.mistakes.maximum = 9;
    if (!ruleMatches(lifetimeExact, context)) return false;

    FakeRandom rng;
    return resolveEvolution(context, rng) == FormId::AdultA;
}

bool sameStageHistory(const Pet& left, const Pet& right) {
    using neripal::core::EvolutionStage;
    const EvolutionStage stages[] = {
        EvolutionStage::Egg, EvolutionStage::Baby, EvolutionStage::Child,
        EvolutionStage::Adult, EvolutionStage::Final,
    };
    for (const auto stage : stages) {
        const auto& a = neripal::core::historyFor(left.careRecord(), stage);
        const auto& b = neripal::core::historyFor(right.careRecord(), stage);
        if (a.steps != b.steps || a.healthGoodSteps != b.healthGoodSteps ||
            a.healthPoorSteps != b.healthPoorSteps || a.happinessGoodSteps != b.happinessGoodSteps ||
            a.happinessPoorSteps != b.happinessPoorSteps || a.careMistakes != b.careMistakes ||
            a.trainCount != b.trainCount) {
            return false;
        }
    }
    if (left.careRecord().lifetimeCareMistakes != right.careRecord().lifetimeCareMistakes) {
        return false;
    }
    for (std::size_t index = 0; index < neripal::core::kBaseNeedCount; ++index) {
        const auto& a = left.careRecord().episodes[index];
        const auto& b = right.careRecord().episodes[index];
        if (a.state != b.state || a.stepsRemaining != b.stepsRemaining) return false;
    }
    return true;
}

bool sameNotices(const Pet& left, const Pet& right) {
    const auto a = left.capture();
    const auto b = right.capture();
    if (a.noticeCount != b.noticeCount) return false;
    for (std::uint8_t index = 0; index < a.noticeCount; ++index) {
        if (a.notices[index].from != b.notices[index].from ||
            a.notices[index].to != b.notices[index].to ||
            a.notices[index].form != b.notices[index].form) {
            return false;
        }
    }
    return true;
}

bool sameOfflineResult(const Pet& left, const Pet& right) {
    const auto a = left.capture();
    const auto b = right.capture();
    return sameNeeds(a, b) && sameCareStats(left.state(), right.state()) &&
           a.needsStepPhase == b.needsStepPhase && a.stage == b.stage && a.form == b.form &&
           a.ageMillis == b.ageMillis && a.sleepCause == b.sleepCause && sameStageHistory(left, right) &&
           sameNotices(left, right);
}

std::uint32_t stageStepCount(const Pet& pet, neripal::core::EvolutionStage stage) {
    return neripal::core::historyFor(pet.careRecord(), stage).steps;
}

bool offlineWithoutCrossingKeepsStage() {
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    pet.applyOffline(30 * B::kNeedsStepMs, 30 * B::kNeedsStepMs);
    return pet.state().stage == neripal::core::EvolutionStage::Baby &&
           pet.state().form == neripal::core::FormId::Juvenile &&
           pet.state().ageMillis == 30 * B::kNeedsStepMs && pet.pendingEvolutionNotices() == 0;
}

bool offlineEggHatches() {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    const int hunger = pet.state().hunger;
    pet.applyOffline(E::kEggHatchAgeMs, E::kEggHatchAgeMs);
    neripal::core::EvolutionNotice notice{};
    GameEvent event{};
    while (pet.pollEvent(event)) {
        if (event.kind == GameEventKind::Hatched) return false;
    }
    return pet.state().stage == neripal::core::EvolutionStage::Baby &&
           pet.state().form == neripal::core::FormId::Juvenile && pet.state().hunger == hunger &&
           pet.pendingEvolutionNotices() == 1 && pet.peekEvolutionNotice(notice) &&
           notice.from == neripal::core::EvolutionStage::Egg &&
           notice.to == neripal::core::EvolutionStage::Baby &&
           notice.form == neripal::core::FormId::Juvenile &&
           stageStepCount(pet, neripal::core::EvolutionStage::Egg) == 0 &&
           stageStepCount(pet, neripal::core::EvolutionStage::Baby) == 0;
}

bool offlineBabyBecomesChild() {
    namespace E = neripal::core::evolution;
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    pet.applyOffline(E::kChildAgeMs, E::kChildAgeMs);
    neripal::core::EvolutionNotice notice{};
    return pet.state().stage == neripal::core::EvolutionStage::Child &&
           pet.state().form == neripal::core::FormId::Juvenile &&
           stageStepCount(pet, neripal::core::EvolutionStage::Baby) ==
               static_cast<std::uint32_t>(E::kChildAgeMs / B::kNeedsStepMs) &&
           stageStepCount(pet, neripal::core::EvolutionStage::Child) == 0 &&
           pet.pendingEvolutionNotices() == 1 && pet.peekEvolutionNotice(notice) &&
           notice.from == neripal::core::EvolutionStage::Baby &&
           notice.to == neripal::core::EvolutionStage::Child &&
           notice.form == neripal::core::FormId::Juvenile;
}

bool offlineChildBecomesAdultOnce() {
    namespace E = neripal::core::evolution;
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    auto snap = childAtAdultGate(0, 18, 100, 100, 100);
    snap.ageMillis = E::kAdultAgeMs - B::kNeedsStepMs;
    pet.restoreSnapshot(snap);
    pet.applyOffline(B::kNeedsStepMs, B::kNeedsStepMs);
    if (pet.state().stage != neripal::core::EvolutionStage::Adult) return false;
    if (pet.state().form != neripal::core::FormId::AdultA) return false;
    if (rng.remaining() != 0) return false;
    if (stageStepCount(pet, neripal::core::EvolutionStage::Child) != 101) return false;
    if (stageStepCount(pet, neripal::core::EvolutionStage::Adult) != 0) return false;
    clock.advance(B::kNeedsStepMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Adult &&
           pet.state().form == neripal::core::FormId::AdultA && rng.remaining() == 0 &&
           pet.pendingEvolutionNotices() == 1;
}

bool offlineAdultBecomesFinalKeepsForm() {
    namespace E = neripal::core::evolution;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    auto snap = pet.capture();
    snap.stage = neripal::core::EvolutionStage::Adult;
    snap.form = neripal::core::FormId::AdultB;
    snap.ageMillis = E::kAdultAgeMs;
    pet.restoreSnapshot(snap);
    const auto gap = E::kFinalAgeMs - E::kAdultAgeMs;
    pet.applyOffline(gap, gap);
    neripal::core::EvolutionNotice notice{};
    return pet.state().stage == neripal::core::EvolutionStage::Final &&
           pet.state().form == neripal::core::FormId::AdultB && rng.remaining() == 0 &&
           pet.pendingEvolutionNotices() == 1 && pet.peekEvolutionNotice(notice) &&
           notice.from == neripal::core::EvolutionStage::Adult &&
           notice.to == neripal::core::EvolutionStage::Final &&
           notice.form == neripal::core::FormId::AdultB;
}

bool offlineLongJumpCrossesInOrder() {
    namespace E = neripal::core::evolution;
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet(clock, rng);
    pet.applyOffline(E::kFinalAgeMs, E::kFinalAgeMs);
    if (pet.state().stage != neripal::core::EvolutionStage::Final) return false;
    if (rng.remaining() != 0) return false;
    if (stageStepCount(pet, neripal::core::EvolutionStage::Egg) != 0) return false;
    if (stageStepCount(pet, neripal::core::EvolutionStage::Baby) !=
        static_cast<std::uint32_t>((E::kChildAgeMs - E::kEggHatchAgeMs) / B::kNeedsStepMs)) {
        return false;
    }
    if (stageStepCount(pet, neripal::core::EvolutionStage::Child) !=
        static_cast<std::uint32_t>((E::kAdultAgeMs - E::kChildAgeMs) / B::kNeedsStepMs)) {
        return false;
    }
    if (stageStepCount(pet, neripal::core::EvolutionStage::Adult) !=
        static_cast<std::uint32_t>((E::kFinalAgeMs - E::kAdultAgeMs) / B::kNeedsStepMs)) {
        return false;
    }
    if (stageStepCount(pet, neripal::core::EvolutionStage::Final) != 0) return false;

    const auto saved = pet.capture();
    if (saved.noticeCount != 4) return false;
    const neripal::core::EvolutionStage expectedFrom[] = {
        neripal::core::EvolutionStage::Egg, neripal::core::EvolutionStage::Baby,
        neripal::core::EvolutionStage::Child, neripal::core::EvolutionStage::Adult,
    };
    const neripal::core::EvolutionStage expectedTo[] = {
        neripal::core::EvolutionStage::Baby, neripal::core::EvolutionStage::Child,
        neripal::core::EvolutionStage::Adult, neripal::core::EvolutionStage::Final,
    };
    for (std::uint8_t index = 0; index < 4; ++index) {
        if (saved.notices[index].from != expectedFrom[index] ||
            saved.notices[index].to != expectedTo[index]) {
            return false;
        }
    }
    if (saved.notices[0].form != neripal::core::FormId::Juvenile) return false;
    if (saved.notices[1].form != neripal::core::FormId::Juvenile) return false;
    if (saved.notices[2].form != pet.state().form || saved.notices[3].form != pet.state().form) {
        return false;
    }
    const auto stage = pet.state().stage;
    const auto form = pet.state().form;
    for (int index = 0; index < 4; ++index) {
        if (!pet.confirmEvolutionNotice()) return false;
    }
    return pet.pendingEvolutionNotices() == 0 && pet.state().stage == stage &&
           pet.state().form == form && !pet.confirmEvolutionNotice();
}

bool offlineHistorySplitsAcrossStages() {
    namespace E = neripal::core::evolution;
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng;
    Pet pet = hatchedPet(clock, rng);
    auto state = pet.state();
    state.ageMillis = E::kChildAgeMs - 2 * B::kNeedsStepMs;
    pet.restore(state);
    pet.applyOffline(5 * B::kNeedsStepMs, 5 * B::kNeedsStepMs);
    return pet.state().stage == neripal::core::EvolutionStage::Child &&
           stageStepCount(pet, neripal::core::EvolutionStage::Baby) == 2 &&
           stageStepCount(pet, neripal::core::EvolutionStage::Child) == 3 &&
           pet.pendingEvolutionNotices() == 1;
}

bool offlineUrgentLeavesWindowPaused() {
    namespace B = neripal::core::balance;
    using neripal::core::EpisodeState;
    FakeClock clock;
    XorShift32 rng(1u);
    Pet pet = hatchedPet(clock, rng);
    pet.restore(watchedHunger(B::kHungerUrgent));
    advanceMinutes(clock, pet, 1);
    const int hygiene = pet.state().hygiene;
    const auto& before = pet.careRecord().episodes[0];
    if (before.state != EpisodeState::Open || before.stepsRemaining != B::kAttentionWindowSteps) {
        return false;
    }
    pet.applyOffline(2ull * 60 * B::kNeedsStepMs, 2ull * 60 * B::kNeedsStepMs);
    const auto& episode = pet.careRecord().episodes[0];
    return episode.state == EpisodeState::Open &&
           episode.stepsRemaining == B::kAttentionWindowSteps &&
           pet.careRecord().lifetimeCareMistakes == 0 &&
           neripal::core::historyFor(pet.careRecord(), neripal::core::EvolutionStage::Baby)
                   .careMistakes == 0 &&
           pet.state().hygiene < hygiene;
}

bool offlineSpecialRollIsNotRepeated() {
    namespace E = neripal::core::evolution;
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom rng(std::vector<std::uint32_t>{0u});
    Pet pet = hatchedPet(clock, rng);
    auto snap = childAtAdultGate(0, 6, 100, 100, 100);
    snap.ageMillis = E::kAdultAgeMs - B::kNeedsStepMs;
    snap.hunger = 10;
    snap.energy = 80;
    snap.hygiene = 80;
    snap.health = 90;
    snap.happiness = 90;
    snap.affection = 80;
    snap.stimulation = 80;
    pet.restoreSnapshot(snap);
    pet.applyOffline(B::kNeedsStepMs, B::kNeedsStepMs);
    if (pet.state().stage != neripal::core::EvolutionStage::Adult) return false;
    if (pet.state().form != neripal::core::FormId::AdultSecret) return false;
    if (rng.remaining() != 0) return false;
    neripal::core::EvolutionNotice notice{};
    if (pet.pendingEvolutionNotices() != 1 || !pet.peekEvolutionNotice(notice)) return false;
    if (notice.from != neripal::core::EvolutionStage::Child ||
        notice.to != neripal::core::EvolutionStage::Adult ||
        notice.form != neripal::core::FormId::AdultSecret) {
        return false;
    }

    const auto saved = pet.capture();
    pet.reset();
    pet.restoreSnapshot(saved);
    if (pet.state().form != neripal::core::FormId::AdultSecret || pet.pendingEvolutionNotices() != 1) {
        return false;
    }
    clock.advance(B::kNeedsStepMs);
    pet.update();
    if (pet.state().stage != neripal::core::EvolutionStage::Adult ||
        pet.state().form != neripal::core::FormId::AdultSecret || rng.remaining() != 0 ||
        pet.pendingEvolutionNotices() != 1) {
        return false;
    }
    const auto stage = pet.state().stage;
    const auto form = pet.state().form;
    if (!pet.confirmEvolutionNotice()) return false;
    return pet.pendingEvolutionNotices() == 0 && pet.state().stage == stage && pet.state().form == form;
}

bool offlineMatchesMinuteByMinute() {
    namespace E = neripal::core::evolution;
    namespace B = neripal::core::balance;
    FakeClock clock;
    FakeRandom fastRng;
    FakeRandom slowRng;
    Pet fast = hatchedPet(clock, fastRng);
    Pet slow = hatchedPet(clock, slowRng);
    const auto gap = 3ull * 60 * B::kNeedsStepMs;
    fast.applyOffline(gap, gap);
    for (int minute = 0; minute < 180; ++minute) {
        slow.applyOffline(B::kNeedsStepMs, B::kNeedsStepMs);
    }
    if (!sameOfflineResult(fast, slow)) return false;

    FakeRandom fastCrossRng;
    FakeRandom slowCrossRng;
    Pet fastCross = hatchedPet(clock, fastCrossRng);
    Pet slowCross = hatchedPet(clock, slowCrossRng);
    auto snap = fastCross.capture();
    snap.ageMillis = E::kChildAgeMs - 3 * B::kNeedsStepMs;
    fastCross.restoreSnapshot(snap);
    slowCross.restoreSnapshot(snap);
    fastCross.applyOffline(8 * B::kNeedsStepMs, 8 * B::kNeedsStepMs);
    for (int minute = 0; minute < 8; ++minute) {
        slowCross.applyOffline(B::kNeedsStepMs, B::kNeedsStepMs);
    }
    if (!sameOfflineResult(fastCross, slowCross)) return false;
    if (fastCross.state().stage != neripal::core::EvolutionStage::Child) return false;

    FakeRandom fastDayRng;
    FakeRandom slowDayRng;
    Pet fastDay = hatchedPet(clock, fastDayRng);
    Pet slowDay = hatchedPet(clock, slowDayRng);
    const int dayMinutes = 24 * 60;
    fastDay.applyOffline(E::kAdultAgeMs, E::kAdultAgeMs);
    for (int minute = 0; minute < dayMinutes; ++minute) {
        slowDay.applyOffline(B::kNeedsStepMs, B::kNeedsStepMs);
    }
    return sameOfflineResult(fastDay, slowDay) &&
           fastDay.state().stage == neripal::core::EvolutionStage::Adult &&
           fastDayRng.remaining() == 0 && slowDayRng.remaining() == 0;
}

}  // namespace

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
        {"high hunger lowers happiness only when urgent", highHungerLowersHappinessOnlyWhenUrgent},
        {"low hygiene lowers happiness only when urgent", lowHygieneLowersHappinessOnlyWhenUrgent},
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
        {"capture restore player sleep", captureRestorePlayerSleep},
        {"capture restore nap keeps remaining", captureRestoreNapKeepsRemaining},
        {"capture does not persist walk pose", captureDoesNotPersistWalkPose},
        {"restore snapshot preserves needs remainder", restoreSnapshotPreservesNeedsRemainder},
        {"restore snapshot zero nap remaining wakes", restoreSnapshotZeroNapRemainingWakes},
        {"restore snapshot oversized nap wakes", restoreSnapshotOversizedNapWakes},
        {"restore snapshot does not consume rng", restoreSnapshotDoesNotConsumeRng},
        {"restore snapshot clears queued events", restoreSnapshotClearsQueuedEvents},
        {"restored nap ends when energy at wake threshold", restoredNapEndsWhenEnergyAtWakeThreshold},
        {"offline zero does not change needs", offlineZeroDoesNotChangeNeeds},
        {"offline thirty seconds keeps remainder", offlineThirtySecondsKeepsRemainder},
        {"offline ninety seconds applies one need step", offlineNinetySecondsAppliesOneNeedStep},
        {"offline uses existing remainder", offlineUsesExistingRemainder},
        {"offline player sleep stays asleep", offlinePlayerSleepStaysAsleep},
        {"offline nap continues", offlineNapContinues},
        {"offline nap ends by time then awake", offlineNapEndsByTimeThenAwake},
        {"offline nap ends early when energy reaches wake", offlineNapEndsEarlyWhenEnergyReachesWake},
        {"offline forty five days caps needs only", offlineFortyFiveDaysCapsNeedsOnly},
        {"offline four hundred days caps age and needs", offlineFourHundredDaysCapsAgeAndNeeds},
        {"offline does not consume rng or walk", offlineDoesNotConsumeRngOrWalk},
        {"offline settle matches minute oracle", offlineSettleMatchesMinuteOracle},
        {"offline player sleep settle matches minute oracle", offlinePlayerSleepSettleMatchesMinuteOracle},
        {"offline nap split settle matches minute oracle", offlineNapSplitSettleMatchesMinuteOracle},
        {"need levels follow provisional thresholds", needLevelsFollowProvisionalThresholds},
        {"one urgent drops happiness once", oneUrgentDropsHappinessOnce},
        {"affection urgent does not lower health", affectionUrgentDoesNotLowerHealth},
        {"awake affection and stimulation follow cadence", awakeAffectionAndStimulationFollowCadence},
        {"sleep slows hygiene and affection and holds stimulation", sleepSlowsHygieneAndAffectionAndHoldsStimulation},
        {"pet raises affection without stimulation", petRaisesAffectionWithoutStimulation},
        {"play raises stimulation without affection", playRaisesStimulationWithoutAffection},
        {"train raises stimulation without affection", trainRaisesStimulationWithoutAffection},
        {"play rejected without energy", playRejectedWithoutEnergy},
        {"pet rejected when asleep", petRejectedWhenAsleep},
        {"capture restores affection and needs phase", captureRestoresAffectionAndNeedsPhase},
        {"attention does not open care episode", attentionDoesNotOpenCareEpisode},
        {"urgent opens episode without mistake", urgentOpensEpisodeWithoutMistake},
        {"care before timeout adds no mistake", careBeforeTimeoutAddsNoMistake},
        {"care timeout adds exactly one mistake", careTimeoutAddsExactlyOneMistake},
        {"staying urgent does not add another mistake", stayingUrgentDoesNotAddAnotherMistake},
        {"leaving urgent closes episode", leavingUrgentClosesEpisode},
        {"reentering urgent opens new episode", reenteringUrgentOpensNewEpisode},
        {"sleep pauses attention window", sleepPausesAttentionWindow},
        {"offline does not touch window or mistakes", offlineDoesNotTouchWindowOrMistakes},
        {"response time counts open minutes", responseTimeCountsOpenMinutes},
        {"train increments training count and play does not", trainIncrementsTrainingCountAndPlayDoesNot},
        {"stage history is kept when age moves stage", stageHistoryIsKeptWhenAgeMovesStage},
        {"new game starts as egg", newGameStartsAsEgg},
        {"egg does not change before hatch", eggDoesNotChangeBeforeHatch},
        {"egg hatches at threshold", eggHatchesAtThreshold},
        {"baby becomes child at age", babyBecomesChildAtAge},
        {"child becomes adult at age", childBecomesAdultAtAge},
        {"adult becomes final at age", adultBecomesFinalAtAge},
        {"long update does not skip stages", longUpdateDoesNotSkipStages},
        {"overdue age advances only one stage", overdueAgeAdvancesOnlyOneStage},
        {"restore keeps persisted stage", restoreKeepsPersistedStage},
        {"restore snapshot keeps persisted stage", restoreSnapshotKeepsPersistedStage},
        {"egg does not degrade stats", eggDoesNotDegradeStats},
        {"egg does not open episodes or history", eggDoesNotOpenEpisodesOrHistory},
        {"care actions rejected on egg", careActionsRejectedOnEgg},
        {"stage change starts new history and keeps previous", stageChangeStartsNewHistoryAndKeepsPrevious},
        {"child to adult follows fixture rules", childToAdultFollowsFixtureRules},
        {"special rule picks secret or adult b", specialRulePicksSecretOrAdultB},
        {"higher priority rule wins", higherPriorityRuleWins},
        {"fallback resolves empty history", fallbackResolvesEmptyHistory},
        {"identical raising without rng matches", identicalRaisingWithoutRngMatches},
        {"evolution rng consumed once", evolutionRngConsumedOnce},
        {"later tick keeps form", laterTickKeepsForm},
        {"restore snapshot keeps form without reroll", restoreSnapshotKeepsFormWithoutReroll},
        {"adult to final keeps form", adultToFinalKeepsForm},
        {"evolution context reads other stage and lifetime", evolutionContextReadsOtherStageAndLifetime},
        {"offline without crossing keeps stage", offlineWithoutCrossingKeepsStage},
        {"offline egg hatches", offlineEggHatches},
        {"offline baby becomes child", offlineBabyBecomesChild},
        {"offline child becomes adult once", offlineChildBecomesAdultOnce},
        {"offline adult becomes final and keeps form", offlineAdultBecomesFinalKeepsForm},
        {"offline long jump crosses in order", offlineLongJumpCrossesInOrder},
        {"offline history splits across stages", offlineHistorySplitsAcrossStages},
        {"offline urgent leaves window paused", offlineUrgentLeavesWindowPaused},
        {"offline special roll is not repeated", offlineSpecialRollIsNotRepeated},
        {"offline matches minute by minute", offlineMatchesMinuteByMinute},
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
