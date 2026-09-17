#include "neripal/ui/UiController.hpp"

#include <algorithm>

namespace neripal::ui {
namespace {
constexpr std::uint64_t kIdleFrameMillis = 500;
constexpr std::uint64_t kMenuSelectionMillis = 120;

constexpr int kMenuFeed = 0;
constexpr int kMenuTrain = 1;
constexpr int kMenuSleep = 2;
constexpr int kMenuClean = 3;
constexpr int kMenuStatus = 4;
constexpr int kMenuHome = 5;
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

void UiController::confirmMenuSelection(const core::PetState& petState) {
    switch (state_.menuIndex) {
        case kMenuFeed:
            pendingCareAction_ = core::CareAction::Feed;
            state_.screen = Screen::Home;
            break;
        case kMenuTrain:
            pendingCareAction_ = core::CareAction::Train;
            state_.screen = Screen::Home;
            break;
        case kMenuSleep:
            pendingCareAction_ =
                petState.sleeping ? core::CareAction::Wake : core::CareAction::Sleep;
            state_.screen = Screen::Home;
            break;
        case kMenuClean:
            pendingCareAction_ = core::CareAction::Clean;
            state_.screen = Screen::Home;
            break;
        case kMenuStatus:
            state_.screen = Screen::Status;
            break;
        case kMenuHome:
            state_.screen = Screen::Home;
            break;
        default:
            break;
    }
}

void UiController::handleInput(platform::InputAction action, const core::PetState& petState) {
    switch (state_.screen) {
        case Screen::Home:
            if (action == platform::InputAction::Confirm) openMenu();
            break;
        case Screen::MainMenu:
            if (action == platform::InputAction::Next) {
                selectNext();
            } else if (action == platform::InputAction::Back) {
                state_.screen = Screen::Home;
            } else if (action == platform::InputAction::Confirm) {
                confirmMenuSelection(petState);
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

std::optional<core::CareAction> UiController::takeCareAction() {
    auto pending = pendingCareAction_;
    pendingCareAction_.reset();
    return pending;
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
