#include "FakeClock.hpp"
#include "neripal/core/Balance.hpp"
#include "neripal/core/Care.hpp"
#include "neripal/core/Pet.hpp"

#include <functional>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
using neripal::core::CareResult;
using neripal::core::Pet;
using neripal::core::PetState;

struct TestCase { std::string_view name; std::function<bool()> run; };

bool sameCareStats(const PetState& a, const PetState& b) {
    return a.hunger == b.hunger && a.happiness == b.happiness && a.energy == b.energy &&
           a.health == b.health && a.hygiene == b.hygiene && a.sleeping == b.sleeping;
}

bool feedReducesHunger() {
    FakeClock clock; Pet pet(clock); const int before = pet.state().hunger;
    return pet.feed() == CareResult::Applied && pet.state().hunger < before;
}
bool hungerNeverBelowZero() {
    FakeClock clock; Pet pet(clock); for (int i = 0; i < 20; ++i) pet.feed();
    return pet.state().hunger == 0;
}
bool trainingConsumesEnergy() {
    FakeClock clock; Pet pet(clock); const int before = pet.state().energy;
    return pet.train() == CareResult::Applied && pet.state().energy < before;
}
bool sleepingRecoversEnergy() {
    FakeClock clock; Pet pet(clock); auto state = pet.state(); state.energy = 50;
    pet.restore(state); pet.sleep(); clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update(); return pet.state().energy > 50;
}
bool energyNeverAbove100() {
    FakeClock clock; Pet pet(clock); auto state = pet.state(); state.energy = 99;
    pet.restore(state); pet.sleep(); clock.advance(neripal::core::balance::kNeedsStepMs * 10);
    pet.update(); return pet.state().energy == 100;
}
bool happinessIsClamped() {
    FakeClock clock; Pet pet(clock); auto state = pet.state(); state.happiness = 999;
    pet.restore(state); if (pet.state().happiness != 100) return false;
    state.happiness = -50; pet.restore(state); return pet.state().happiness == 0;
}
bool healthIsClamped() {
    FakeClock clock; Pet pet(clock); auto state = pet.state(); state.health = -1;
    pet.restore(state); if (pet.state().health != 0) return false;
    state.health = 101; pet.restore(state); return pet.state().health == 100;
}
bool controlledTimeIncreasesHunger() {
    FakeClock clock; Pet pet(clock); const int before = pet.state().hunger;
    clock.advance(neripal::core::balance::kNeedsStepMs); pet.update();
    return pet.state().hunger == before + neripal::core::balance::kHungerPerStep;
}
bool controlledTimeChangesEnergyByState() {
    FakeClock clock; Pet pet(clock); const int awake = pet.state().energy;
    clock.advance(neripal::core::balance::kNeedsStepMs); pet.update();
    if (pet.state().energy >= awake) return false;
    const int beforeSleep = pet.state().energy; pet.sleep();
    clock.advance(neripal::core::balance::kNeedsStepMs); pet.update();
    return pet.state().energy > beforeSleep;
}
bool noRealWaitIsNeeded() {
    FakeClock clock; Pet pet(clock);
    clock.advance(3 * neripal::core::balance::kNeedsStepMs); pet.update();
    return pet.state().ageMillis == 3 * neripal::core::balance::kNeedsStepMs;
}
bool restoreNormalizesEveryStat() {
    FakeClock clock; Pet pet(clock);
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
    FakeClock clock; Pet pet(clock);
    PetState invalid = pet.state();
    invalid.hygiene = -8;
    pet.restore(invalid);
    return pet.state().hygiene == 0;
}
bool evolutionUsesControlledAge() {
    FakeClock clock; Pet pet(clock);
    clock.advance(neripal::core::evolution::kChildAgeMs); pet.update();
    if (pet.state().stage != neripal::core::EvolutionStage::Child) return false;
    clock.advance(neripal::core::evolution::kAdultAgeMs -
                  neripal::core::evolution::kChildAgeMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Adult;
}
bool eggHatchesWithControlledAge() {
    FakeClock clock; Pet pet(clock); auto state = pet.state();
    state.stage = neripal::core::EvolutionStage::Egg;
    state.ageMillis = 0;
    pet.restore(state);
    if (pet.state().stage != neripal::core::EvolutionStage::Egg) return false;
    clock.advance(neripal::core::evolution::kEggHatchAgeMs);
    pet.update();
    return pet.state().stage == neripal::core::EvolutionStage::Baby;
}
bool cleanRaisesHygiene() {
    FakeClock clock; Pet pet(clock);
    auto state = pet.state();
    state.hygiene = 40;
    pet.restore(state);
    return pet.clean() == CareResult::Applied &&
           pet.state().hygiene == 40 + neripal::core::balance::kCleanHygiene;
}
bool cleanDoesNotExceedMax() {
    FakeClock clock; Pet pet(clock);
    auto state = pet.state();
    state.hygiene = 90;
    pet.restore(state);
    return pet.clean() == CareResult::Applied && pet.state().hygiene == 100;
}
bool awakeTimeLowersHygiene() {
    FakeClock clock; Pet pet(clock);
    const int before = pet.state().hygiene;
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().hygiene == before + neripal::core::balance::kHygienePerAwakeStep;
}
bool sleepDoesNotLowerHygiene() {
    FakeClock clock; Pet pet(clock);
    auto state = pet.state();
    state.hygiene = 50;
    pet.restore(state);
    pet.sleep();
    clock.advance(neripal::core::balance::kNeedsStepMs);
    pet.update();
    return pet.state().hygiene == 50;
}
bool highHungerLowersHappiness() {
    FakeClock clock; Pet pet(clock);
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
    FakeClock clock; Pet pet(clock);
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
    FakeClock clock; Pet pet(clock);
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
    FakeClock clock; Pet pet(clock);
    auto state = pet.state();
    state.sleeping = true;
    pet.restore(state);
    const auto before = pet.state();
    return pet.feed() == CareResult::RejectedAsleep && sameCareStats(before, pet.state());
}
bool trainRejectedWhenAsleep() {
    FakeClock clock; Pet pet(clock);
    auto state = pet.state();
    state.sleeping = true;
    state.energy = 5;
    pet.restore(state);
    const auto before = pet.state();
    return pet.train() == CareResult::RejectedAsleep && sameCareStats(before, pet.state());
}
bool cleanRejectedWhenAsleep() {
    FakeClock clock; Pet pet(clock);
    auto state = pet.state();
    state.sleeping = true;
    pet.restore(state);
    const auto before = pet.state();
    return pet.clean() == CareResult::RejectedAsleep && sameCareStats(before, pet.state());
}
bool trainRejectedWithoutEnergy() {
    FakeClock clock; Pet pet(clock);
    auto state = pet.state();
    state.energy = -neripal::core::balance::kTrainEnergy - 1;
    pet.restore(state);
    const auto before = pet.state();
    return pet.train() == CareResult::RejectedNoEnergy && sameCareStats(before, pet.state());
}
bool sleepRejectedWhenAlreadySleeping() {
    FakeClock clock; Pet pet(clock);
    pet.sleep();
    const auto before = pet.state();
    return pet.sleep() == CareResult::RejectedAlreadySleeping &&
           sameCareStats(before, pet.state());
}
bool wakeRejectedWhenAlreadyAwake() {
    FakeClock clock; Pet pet(clock);
    const auto before = pet.state();
    return pet.wake() == CareResult::RejectedAlreadyAwake && sameCareStats(before, pet.state());
}
bool applyDispatchesCareActions() {
    using neripal::core::CareAction;
    FakeClock clock; Pet pet(clock);
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
    };

    int failures = 0;
    for (const auto& test : tests) {
        const bool passed = test.run();
        std::cout << (passed ? "[PASS] " : "[FAIL] ") << test.name << '\n';
        failures += passed ? 0 : 1;
    }
    std::cout << tests.size() - failures << '/' << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
