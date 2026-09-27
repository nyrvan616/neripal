#pragma once

#include "neripal/core/Care.hpp"
#include "neripal/core/EvolutionNotice.hpp"
#include "neripal/core/PetState.hpp"
#include "neripal/platform/IInput.hpp"

#include <cstdint>
#include <optional>

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
    bool careFeedbackActive = false;
    core::CareAction careAction = core::CareAction::Feed;
    core::CareResult careResult = core::CareResult::Applied;
    // Copied from a peeked EvolutionNotice after it is shown. Confirming it
    // does not change stage or form; the composition root calls
    // Pet::confirmEvolutionNotice() only after takeEvolutionConfirm().
    bool evolutionNoticeVisible = false;
    core::EvolutionStage evolutionFrom = core::EvolutionStage::Egg;
    core::EvolutionStage evolutionTo = core::EvolutionStage::Egg;
    core::FormId evolutionForm = core::FormId::None;
};

// Owns transient presentation state only. It never mutates the game Core.
class UiController {
public:
    static constexpr int kMenuFeed = 0;
    static constexpr int kMenuTrain = 1;
    static constexpr int kMenuSleep = 2;
    static constexpr int kMenuClean = 3;
    static constexpr int kMenuPet = 4;
    static constexpr int kMenuPlay = 5;
    static constexpr int kMenuStatus = 6;
    static constexpr int kMenuHome = 7;
    static constexpr int kMenuItemCount = 8;
    static constexpr std::uint64_t kMenuSelectionMillis = 240;
    static constexpr std::uint64_t kCareFeedbackMillis = 900;

    const UiState& state() const noexcept { return state_; }

    void handleInput(platform::InputAction action, const core::PetState& petState);
    void update(std::uint64_t nowMillis);
    std::optional<core::CareAction> takeCareAction();
    void beginCareFeedback(core::CareAction action, core::CareResult result,
                           std::uint64_t nowMillis);

    // Shows one notice. Does not remove it from the pet. A second call while
    // one is already visible is ignored so the queue stays in order.
    void presentEvolutionNotice(const core::EvolutionNotice& notice);
    bool showingEvolutionNotice() const noexcept { return state_.evolutionNoticeVisible; }
    // True only after the user confirms the notice that is already on screen.
    bool takeEvolutionConfirm();

private:
    void openMenu();
    void selectNext();
    void confirmMenuSelection(const core::PetState& petState);

    UiState state_{};
    std::optional<core::CareAction> pendingCareAction_;
    bool pendingEvolutionConfirm_ = false;
    std::uint64_t lastNowMillis_ = 0;
    std::uint64_t selectionStartedMillis_ = 0;
    std::uint64_t careFeedbackStartedMillis_ = 0;
    bool hasTime_ = false;
};

}  // namespace neripal::ui
