#include "FakeClock.hpp"
#include "neripal/core/Balance.hpp"
#include "neripal/core/Pet.hpp"

#include <functional>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
using neripal::core::Pet;
using neripal::core::PetState;

struct TestCase { std::string_view name; std::function<bool()> run; };

bool feedReducesHunger() {
    FakeClock clock; Pet pet(clock); const int before = pet.state().hunger;
    pet.feed(); return pet.state().hunger < before;
}
bool hungerNeverBelowZero() {
    FakeClock clock; Pet pet(clock); for (int i = 0; i < 20; ++i) pet.feed();
    return pet.state().hunger == 0;
}
bool trainingConsumesEnergy() {
    FakeClock clock; Pet pet(clock); const int before = pet.state().energy;
    pet.train(); return pet.state().energy < before;
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
    FakeClock clock; Pet pet(clock); PetState invalid{-5, 105, -50, 800, 123, true};
    pet.restore(invalid);
    const auto& state = pet.state();
    return state.hunger == 0 && state.happiness == 100 && state.energy == 0 &&
           state.health == 100 && state.ageMillis == 123 && state.sleeping;
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
        {"evolution uses controlled age", evolutionUsesControlledAge},
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
