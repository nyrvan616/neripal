#pragma once

#include "neripal/core/PetState.hpp"
#include "neripal/platform/IRenderer.hpp"
#include "neripal/ui/UiController.hpp"

namespace neripal::ui {

class PetView {
public:
    void render(platform::IRenderer& renderer, const core::PetState& state,
                const UiState& uiState) const;

private:
    static void drawPet(platform::IRenderer& renderer, int x, int y, int idleFrame,
                        int extraBob, bool sleeping, core::EvolutionStage stage);
    static void drawStat(platform::IRenderer& renderer, int y, const char* label,
                         int value, platform::Color color);
};

}  // namespace neripal::ui
