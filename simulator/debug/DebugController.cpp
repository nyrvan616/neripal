#include "DebugController.hpp"

#include "neripal/core/Evolution.hpp"

namespace neripal::simulator {

void DebugController::restore(core::PetState state) {
    pet_.restore(state);
    saves_.markDirty();
}
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
        case 5: state.affection += delta; break;
        case 6: state.stimulation += delta; break;
        default: return;
    }
    restore(state);
}

void DebugController::reset() {
    pet_.reset();
    saves_.saveNow();
}

bool isAdultForm(core::FormId form) noexcept {
    return form == core::FormId::AdultA || form == core::FormId::AdultB ||
           form == core::FormId::AdultC || form == core::FormId::AdultSecret;
}

core::FormId forcedForm(core::EvolutionStage destination, core::FormId current) noexcept {
    switch (destination) {
        case core::EvolutionStage::Egg:
            return core::FormId::None;
        case core::EvolutionStage::Baby:
        case core::EvolutionStage::Child:
            return core::FormId::Juvenile;
        case core::EvolutionStage::Adult:
        case core::EvolutionStage::Final:
            return isAdultForm(current) ? current : core::FormId::AdultC;
    }
    return core::FormId::None;
}

void DebugController::forceEvolution() {
    // Debug write only. AdultC is the deterministic fallback when the current
    // form cannot stay with the destination stage. This is not EvolutionRules.
    auto state = pet_.state();
    switch (state.stage) {
        case core::EvolutionStage::Egg:
            state.stage = core::EvolutionStage::Baby;
            state.ageMillis = 0;
            break;
        case core::EvolutionStage::Baby:
            state.stage = core::EvolutionStage::Child;
            state.ageMillis = core::evolution::kChildAgeMs;
            break;
        case core::EvolutionStage::Child:
            state.stage = core::EvolutionStage::Adult;
            state.ageMillis = core::evolution::kAdultAgeMs;
            break;
        case core::EvolutionStage::Adult:
            state.stage = core::EvolutionStage::Final;
            state.ageMillis = core::evolution::kFinalAgeMs;
            break;
        case core::EvolutionStage::Final:
            state.stage = core::EvolutionStage::Egg;
            state.ageMillis = 0;
            break;
    }
    state.form = forcedForm(state.stage, state.form);
    restore(state);
}

}  // namespace neripal::simulator
