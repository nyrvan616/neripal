#pragma once

#include "neripal/core/Needs.hpp"
#include "neripal/core/PetState.hpp"

#include <cstdint>

namespace neripal::ui {

// One non-normal base need. The symbol is the need itself: Attention and Urgent
// share it. Urgent only raises intensity. Thresholds come from needLevel().
struct NeedSignal {
    core::Need need = core::Need::Hunger;
    core::NeedLevel level = core::NeedLevel::Attention;
};

inline constexpr int kMaxNeedSignals = 5;

struct NeedSignalList {
    NeedSignal items[kMaxNeedSignals]{};
    std::uint8_t count = 0;
    // True when any listed need is Urgent. Presentation may read this to request
    // a future tone. It does not play audio. The drawn signal stays mandatory.
    bool urgentSound = false;
};

inline int statForNeed(const core::PetState& state, core::Need need) noexcept {
    switch (need) {
        case core::Need::Hunger: return state.hunger;
        case core::Need::Energy: return state.energy;
        case core::Need::Hygiene: return state.hygiene;
        case core::Need::Affection: return state.affection;
        case core::Need::Stimulation: return state.stimulation;
    }
    return 0;
}

// Stable token, one per need. Attention and Urgent use the same token.
inline const char* needSymbol(core::Need need) noexcept {
    switch (need) {
        case core::Need::Hunger: return "HUN";
        case core::Need::Energy: return "NRG";
        case core::Need::Hygiene: return "HYG";
        case core::Need::Affection: return "AFE";
        case core::Need::Stimulation: return "STM";
    }
    return "?";
}

inline NeedSignalList needSignals(const core::PetState& state) noexcept {
    NeedSignalList list;
    constexpr core::Need kNeeds[] = {
        core::Need::Hunger, core::Need::Energy, core::Need::Hygiene,
        core::Need::Affection, core::Need::Stimulation};
    for (const core::Need need : kNeeds) {
        const core::NeedLevel level = core::needLevel(need, statForNeed(state, need));
        if (level == core::NeedLevel::Normal) continue;
        list.items[list.count++] = NeedSignal{need, level};
        if (level == core::NeedLevel::Urgent) list.urgentSound = true;
    }
    return list;
}

// Hook for a future urgent tone. Home already draws the visual signal.
// 0.6 does not play audio, and sound is never the only channel.
inline bool urgentSoundRequested(const core::PetState& state) noexcept {
    return needSignals(state).urgentSound;
}

}  // namespace neripal::ui
