#include "DebugController.hpp"

#include "neripal/core/Evolution.hpp"

namespace neripal::simulator {

void DebugController::restore(core::PetState state) { pet_.restore(state); }
void DebugController::setHunger(int value) { auto s = pet_.state(); s.hunger = value; restore(s); }
void DebugController::setHappiness(int value) { auto s = pet_.state(); s.happiness = value; restore(s); }
void DebugController::setEnergy(int value) { auto s = pet_.state(); s.energy = value; restore(s); }
void DebugController::setHealth(int value) { auto s = pet_.state(); s.health = value; restore(s); }
void DebugController::setHygiene(int value) { auto s = pet_.state(); s.hygiene = value; restore(s); }

void DebugController::adjustStat(int index, int delta) {
    auto state = pet_.state();
    switch (index) {
        case 0: state.hunger += delta; break;
        case 1: state.happiness += delta; break;
        case 2: state.energy += delta; break;
        case 3: state.health += delta; break;
        case 4: state.hygiene += delta; break;
        default: return;
    }
    restore(state);
}

void DebugController::forceEvolution() {
    auto state = pet_.state();
    switch (state.stage) {
        case core::EvolutionStage::Egg:
            state.stage = core::EvolutionStage::Baby;
            state.ageMillis = 0;
            break;
        case core::EvolutionStage::Baby:
            state.ageMillis = core::evolution::kChildAgeMs;
            break;
        case core::EvolutionStage::Child:
            state.ageMillis = core::evolution::kAdultAgeMs;
            break;
        case core::EvolutionStage::Adult:
            state.stage = core::EvolutionStage::Egg;
            state.ageMillis = 0;
            break;
    }
    restore(state);
}

}  // namespace neripal::simulator
