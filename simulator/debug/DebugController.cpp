#include "DebugController.hpp"

namespace neripal::simulator {

void DebugController::restore(core::PetState state) { pet_.restore(state); }
void DebugController::setHunger(int value) { auto s = pet_.state(); s.hunger = value; restore(s); }
void DebugController::setHappiness(int value) { auto s = pet_.state(); s.happiness = value; restore(s); }
void DebugController::setEnergy(int value) { auto s = pet_.state(); s.energy = value; restore(s); }
void DebugController::setHealth(int value) { auto s = pet_.state(); s.health = value; restore(s); }

void DebugController::adjustStat(int index, int delta) {
    auto state = pet_.state();
    switch (index) {
        case 0: state.hunger += delta; break;
        case 1: state.happiness += delta; break;
        case 2: state.energy += delta; break;
        case 3: state.health += delta; break;
        default: return;
    }
    restore(state);
}

}  // namespace neripal::simulator
