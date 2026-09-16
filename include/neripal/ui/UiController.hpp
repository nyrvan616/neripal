#pragma once

#include "neripal/platform/IInput.hpp"

#include <cstdint>

namespace neripal::ui {

enum class Screen {
    Home,
    MainMenu,
    Status,
};

struct UiState {
    Screen screen = Screen::Home;
    int menuIndex = 0;
    int previousMenuIndex = 0;
    int idleFrame = 0;
    float menuSelectionProgress = 1.0F;
};

// Owns transient presentation state only. It never mutates the game Core.
class UiController {
public:
    static constexpr int kMenuItemCount = 2;

    const UiState& state() const noexcept { return state_; }

    void handleInput(platform::InputAction action);
    void update(std::uint64_t nowMillis);

private:
    void openMenu();
    void selectNext();

    UiState state_{};
    std::uint64_t lastNowMillis_ = 0;
    std::uint64_t selectionStartedMillis_ = 0;
    bool hasTime_ = false;
};

}  // namespace neripal::ui
