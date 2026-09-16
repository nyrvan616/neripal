#pragma once

#include "neripal/core/PetState.hpp"
#include "neripal/platform/IRenderer.hpp"

namespace neripal::ui {

class PetView {
public:
    void render(platform::IRenderer& renderer, const core::PetState& state,
                int timeScale = 1, int selectedStat = -1) const;

private:
    static void drawStat(platform::IRenderer& renderer, int y, const char* label,
                         int value, platform::Color color, bool selected);
};

}  // namespace neripal::ui
