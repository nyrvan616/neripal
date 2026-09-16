#include "neripal/ui/UiController.hpp"

#include <algorithm>

namespace neripal::ui {
namespace {
constexpr std::uint64_t kIdleFrameMillis = 500;
constexpr std::uint64_t kMenuSelectionMillis = 120;
}

void UiController::openMenu() {
    state_.screen = Screen::MainMenu;
    state_.previousMenuIndex = state_.menuIndex;
    state_.menuSelectionProgress = 1.0F;
}

void UiController::selectNext() {
    state_.previousMenuIndex = state_.menuIndex;
    state_.menuIndex = (state_.menuIndex + 1) % kMenuItemCount;
    selectionStartedMillis_ = lastNowMillis_;
    state_.menuSelectionProgress = 0.0F;
}

void UiController::handleInput(platform::InputAction action) {
    switch (state_.screen) {
        case Screen::Home:
            if (action == platform::InputAction::Confirm) openMenu();
            break;
        case Screen::MainMenu:
            if (action == platform::InputAction::Next) {
                selectNext();
            } else if (action == platform::InputAction::Back) {
                state_.screen = Screen::Home;
            } else if (state_.menuIndex == 0) {
                state_.screen = Screen::Status;
            } else {
                state_.screen = Screen::Home;
            }
            break;
        case Screen::Status:
            if (action == platform::InputAction::Confirm ||
                action == platform::InputAction::Back) {
                openMenu();
            }
            break;
    }
}

void UiController::update(std::uint64_t nowMillis) {
    if (!hasTime_ || nowMillis < lastNowMillis_) {
        hasTime_ = true;
        lastNowMillis_ = nowMillis;
        selectionStartedMillis_ = nowMillis;
    }

    state_.idleFrame = static_cast<int>((nowMillis / kIdleFrameMillis) % 2ULL);
    if (state_.previousMenuIndex == state_.menuIndex) {
        state_.menuSelectionProgress = 1.0F;
    } else {
        const auto elapsed = nowMillis - selectionStartedMillis_;
        state_.menuSelectionProgress = std::min(
            1.0F, static_cast<float>(elapsed) / static_cast<float>(kMenuSelectionMillis));
        if (state_.menuSelectionProgress >= 1.0F) {
            state_.previousMenuIndex = state_.menuIndex;
        }
    }
    lastNowMillis_ = nowMillis;
}

}  // namespace neripal::ui
