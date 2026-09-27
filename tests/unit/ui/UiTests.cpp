#include "neripal/core/Activity.hpp"
#include "neripal/core/Balance.hpp"
#include "neripal/core/Care.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/core/PetSnapshot.hpp"
#include "neripal/core/PetState.hpp"
#include "neripal/persist/SaveSession.hpp"
#include "neripal/platform/IRenderer.hpp"
#include "neripal/platform/Rgb565.hpp"
#include "neripal/ui/NeedSignals.hpp"
#include "neripal/ui/PetView.hpp"
#include "neripal/ui/UiController.hpp"
#include "platform/desktop/ScaledClock.hpp"

#include "DebugController.hpp"
#include "FakeClock.hpp"
#include "FakeRandom.hpp"
#include "FakeSaveStorage.hpp"
#include "FakeWallClock.hpp"

#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using neripal::core::CareAction;
using neripal::core::CareResult;
using neripal::core::PetState;
using neripal::platform::Color;
using neripal::platform::InputAction;

struct TestCase { std::string_view name; std::function<bool()> run; };

class FakeRenderer final : public neripal::platform::IRenderer {
public:
    struct Rect { int x; int y; int width; int height; };
    struct TextCommand {
        int x = 0;
        int y = 0;
        int scale = 1;
        std::string text;
    };

    void beginFrame(Color) override { beganFrame = true; }
    void fillRect(int x, int y, int width, int height, Color) override {
        rectangles.push_back({x, y, width, height});
    }
    void drawRect(int x, int y, int width, int height, Color) override {
        rectangles.push_back({x, y, width, height});
    }
    void drawText(int x, int y, std::string_view text, Color, int scale) override {
        texts.push_back(TextCommand{x, y, scale, std::string(text)});
    }
    void endFrame() override { endedFrame = true; }

    bool beganFrame = false;
    bool endedFrame = false;
    std::vector<Rect> rectangles;
    std::vector<TextCommand> texts;
};

bool hasText(const FakeRenderer& renderer, std::string_view text) {
    for (const auto& item : renderer.texts) {
        if (item.text == text) return true;
    }
    return false;
}

int effectiveTextScale(int scale) {
    return scale < 1 ? 1 : scale;
}

bool textCommandFits(const FakeRenderer::TextCommand& command, std::string& detail) {
    const int scale = effectiveTextScale(command.scale);
    const int right = command.x + FakeRenderer::kTextCellWidth * scale *
                      static_cast<int>(command.text.size());
    const int bottom = command.y + FakeRenderer::kTextCellHeight * scale;
    if (command.x >= 0 && command.y >= 0 && right <= FakeRenderer::kLogicalWidth &&
        bottom <= FakeRenderer::kLogicalHeight) {
        return true;
    }
    detail = "text=\"" + command.text + "\" x=" + std::to_string(command.x) +
             " y=" + std::to_string(command.y) + " scale=" + std::to_string(command.scale) +
             " effectiveScale=" + std::to_string(scale) + " right=" + std::to_string(right) +
             " bottom=" + std::to_string(bottom);
    return false;
}

bool rectsInside(const FakeRenderer& renderer) {
    for (const auto& rect : renderer.rectangles) {
        if (rect.x < 0 || rect.y < 0 || rect.width < 0 || rect.height < 0 ||
            rect.x + rect.width > FakeRenderer::kLogicalWidth ||
            rect.y + rect.height > FakeRenderer::kLogicalHeight) {
            return false;
        }
    }
    return true;
}

void openMenuAt(neripal::ui::UiController& ui, const PetState& pet, int index) {
    ui.handleInput(InputAction::Confirm, pet);
    for (int i = 0; i < index; ++i) {
        ui.handleInput(InputAction::Next, pet);
    }
}

void syncEvolutionNotice(neripal::ui::UiController& ui, neripal::core::Pet& pet) {
    if (ui.takeEvolutionConfirm()) {
        pet.confirmEvolutionNotice();
    }
    if (!ui.showingEvolutionNotice()) {
        neripal::core::EvolutionNotice notice;
        if (pet.peekEvolutionNotice(notice)) {
            ui.presentEvolutionNotice(notice);
        }
    }
}

bool menuNavigationWrapsEightItems() {
    PetState pet;
    neripal::ui::UiController ui;
    ui.update(0);
    ui.handleInput(InputAction::Confirm, pet);
    if (ui.state().screen != neripal::ui::Screen::MainMenu) return false;
    for (int i = 1; i < neripal::ui::UiController::kMenuItemCount; ++i) {
        ui.handleInput(InputAction::Next, pet);
        if (ui.state().menuIndex != i) return false;
    }
    ui.handleInput(InputAction::Next, pet);
    return ui.state().menuIndex == 0;
}

bool statusReturnsToMenu() {
    PetState pet;
    neripal::ui::UiController ui;
    openMenuAt(ui, pet, neripal::ui::UiController::kMenuStatus);
    if (ui.state().menuIndex != neripal::ui::UiController::kMenuStatus) return false;
    ui.handleInput(InputAction::Confirm, pet);
    if (ui.state().screen != neripal::ui::Screen::Status) return false;
    ui.handleInput(InputAction::Back, pet);
    return ui.state().screen == neripal::ui::Screen::MainMenu;
}

bool feedEnqueuesCareActionAndReturnsHome() {
    PetState pet;
    neripal::ui::UiController ui;
    ui.handleInput(InputAction::Confirm, pet);
    if (ui.state().menuIndex != 0) return false;
    ui.handleInput(InputAction::Confirm, pet);
    if (ui.state().screen != neripal::ui::Screen::Home) return false;
    const auto action = ui.takeCareAction();
    return action.has_value() && *action == CareAction::Feed;
}

bool sleepMenuSelectsWakeWhenSleeping() {
    PetState pet;
    pet.sleeping = true;
    neripal::ui::UiController ui;
    ui.handleInput(InputAction::Confirm, pet);
    ui.handleInput(InputAction::Next, pet);
    ui.handleInput(InputAction::Next, pet);
    if (ui.state().menuIndex != 2) return false;
    ui.handleInput(InputAction::Confirm, pet);
    const auto action = ui.takeCareAction();
    return action.has_value() && *action == CareAction::Wake;
}

bool sleepMenuSelectsSleepWhenAwake() {
    PetState pet;
    neripal::ui::UiController ui;
    ui.handleInput(InputAction::Confirm, pet);
    ui.handleInput(InputAction::Next, pet);
    ui.handleInput(InputAction::Next, pet);
    if (ui.state().menuIndex != 2) return false;
    ui.handleInput(InputAction::Confirm, pet);
    const auto action = ui.takeCareAction();
    return action.has_value() && *action == CareAction::Sleep;
}

bool statusDoesNotEnqueueCareAction() {
    PetState pet;
    neripal::ui::UiController ui;
    openMenuAt(ui, pet, neripal::ui::UiController::kMenuStatus);
    ui.handleInput(InputAction::Confirm, pet);
    return !ui.takeCareAction().has_value();
}

bool menuSelectionEasesOutOver240ms() {
    PetState pet;
    neripal::ui::UiController ui;
    if (neripal::ui::UiController::kMenuSelectionMillis != 240) return false;
    ui.update(0);
    ui.handleInput(InputAction::Confirm, pet);
    ui.handleInput(InputAction::Next, pet);
    if (ui.state().menuIndex != 1 || ui.state().previousMenuIndex != 0) return false;
    if (ui.state().menuSelectionProgress != 0.0F) return false;

    const auto half = neripal::ui::UiController::kMenuSelectionMillis / 2;
    ui.update(half);
    if (ui.state().menuSelectionProgress != 0.5F) return false;
    if (ui.state().previousMenuIndex == ui.state().menuIndex) return false;

    neripal::ui::PetView view;
    FakeRenderer renderer;
    view.render(renderer, pet, ui.state());
    int highlightX = -1;
    int highlightY = -1;
    int highlights = 0;
    for (const auto& rect : renderer.rectangles) {
        if (rect.width != 48 || rect.height != 36) continue;
        highlightX = rect.x;
        highlightY = rect.y;
        ++highlights;
    }
    // Tile 0 is (20, 76) and tile 1 is (72, 76). The 48x42 panel is drawn at
    // origin - 2, and its full-width stroke sits 3 px lower. Linear t=0.5
    // would place that stroke at x=44; the destination stroke is at x=70.
    if (highlights != 1 || highlightY != 77) return false;
    if (highlightX <= 44 || highlightX >= 70) return false;

    ui.update(neripal::ui::UiController::kMenuSelectionMillis - 1);
    if (!(ui.state().menuSelectionProgress < 1.0F)) return false;
    if (ui.state().previousMenuIndex == ui.state().menuIndex) return false;

    ui.update(neripal::ui::UiController::kMenuSelectionMillis);
    return ui.state().menuSelectionProgress == 1.0F &&
           ui.state().previousMenuIndex == ui.state().menuIndex;
}

int footerPills(const FakeRenderer& renderer) {
    int count = 0;
    for (const auto& rect : renderer.rectangles) {
        if (rect.width == 28 && rect.height == 16 && rect.y >= 207) ++count;
    }
    return count;
}

int textX(const FakeRenderer& renderer, std::string_view text) {
    for (const auto& item : renderer.texts) {
        if (item.text == text && item.y == 218) return item.x;
    }
    return -1;
}

bool footerShowsOnlyAcceptedActions() {
    PetState pet;
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;

    const auto show = [&] {
        renderer = FakeRenderer{};
        view.render(renderer, pet, ui.state());
    };
    const auto absent = [&](std::string_view text) { return !hasText(renderer, text); };

    show();
    if (!hasText(renderer, "B") || !hasText(renderer, "Menu")) return false;
    if (footerPills(renderer) != 1) return false;
    if (textX(renderer, "Menu") <= textX(renderer, "B")) return false;
    if (!absent("*") || !absent("+") || !absent("i") || !absent(">") || !absent("MENU")) return false;
    if (!absent("A") || !absent("C") || !absent("Next") || !absent("OK") || !absent("Back")) {
        return false;
    }
    if (!absent("SELECT AN ICON") || !absent("A NEXT") || !absent("C  BACK")) return false;

    ui.handleInput(InputAction::Confirm, pet);
    show();
    if (!hasText(renderer, "A") || !hasText(renderer, "Next") || !hasText(renderer, "B") ||
        !hasText(renderer, "OK") || !hasText(renderer, "C") || !hasText(renderer, "Back")) {
        return false;
    }
    if (footerPills(renderer) != 3) return false;
    if (textX(renderer, "Next") <= textX(renderer, "A")) return false;
    if (textX(renderer, "OK") <= textX(renderer, "B")) return false;
    if (textX(renderer, "Back") <= textX(renderer, "C")) return false;
    if (!absent("SELECT AN ICON") || !absent("Menu") || !absent("*")) return false;

    neripal::ui::UiController status;
    openMenuAt(status, pet, neripal::ui::UiController::kMenuStatus);
    status.handleInput(InputAction::Confirm, pet);
    if (status.state().screen != neripal::ui::Screen::Status) return false;
    renderer = FakeRenderer{};
    view.render(renderer, pet, status.state());
    if (!hasText(renderer, "B") || !hasText(renderer, "Menu") || !hasText(renderer, "C") ||
        !hasText(renderer, "Back")) {
        return false;
    }
    if (footerPills(renderer) != 2) return false;
    if (!absent("A") || !absent("Next") || !absent("OK") || !absent("C  BACK")) return false;

    neripal::ui::UiController notice;
    notice.presentEvolutionNotice(
        {neripal::core::EvolutionStage::Egg, neripal::core::EvolutionStage::Baby,
         neripal::core::FormId::Juvenile});
    renderer = FakeRenderer{};
    view.render(renderer, pet, notice.state());
    if (!hasText(renderer, "B") || !hasText(renderer, "OK") || !hasText(renderer, "EVOLVED")) {
        return false;
    }
    if (footerPills(renderer) != 1) return false;
    if (textX(renderer, "OK") <= textX(renderer, "B")) return false;
    return absent("Menu") && absent("A") && absent("C") && absent("Next") && absent("Back") &&
           absent("B CONFIRM");
}

bool idleAnimationUsesControlledTime() {
    neripal::ui::UiController ui;
    ui.update(0);
    if (ui.state().idleFrame != 0) return false;
    ui.update(500);
    if (ui.state().idleFrame != 1) return false;
    ui.update(1'000);
    return ui.state().idleFrame == 0;
}

bool statusShowsHygieneBar() {
    PetState pet;
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;
    openMenuAt(ui, pet, neripal::ui::UiController::kMenuStatus);
    ui.handleInput(InputAction::Confirm, pet);
    view.render(renderer, pet, ui.state());
    for (const auto& text : renderer.texts) {
        if (text.text == "HYG") return true;
    }
    return false;
}

bool everyScreenStaysInsideLogicalViewport() {
    PetState pet;
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;

    const auto renderAndCheck = [&] {
        renderer = FakeRenderer{};
        view.render(renderer, pet, ui.state());
        if (!renderer.beganFrame || !renderer.endedFrame) return false;
        for (const auto& rect : renderer.rectangles) {
            if (rect.x < 0 || rect.y < 0 || rect.width < 0 || rect.height < 0 ||
                rect.x + rect.width > FakeRenderer::kLogicalWidth ||
                rect.y + rect.height > FakeRenderer::kLogicalHeight) {
                return false;
            }
        }
        return true;
    };

    for (const auto stage : {neripal::core::EvolutionStage::Egg,
                             neripal::core::EvolutionStage::Baby,
                             neripal::core::EvolutionStage::Child,
                             neripal::core::EvolutionStage::Adult,
                             neripal::core::EvolutionStage::Final}) {
        pet.stage = stage;
        if (!renderAndCheck()) return false;
    }
    ui.handleInput(InputAction::Confirm, pet);
    if (!renderAndCheck()) return false;
    for (int i = 0; i < neripal::ui::UiController::kMenuItemCount; ++i) {
        if (!renderAndCheck()) return false;
        ui.handleInput(InputAction::Next, pet);
    }
    ui.handleInput(InputAction::Confirm, pet);
    for (const auto stage : {neripal::core::EvolutionStage::Egg,
                             neripal::core::EvolutionStage::Baby,
                             neripal::core::EvolutionStage::Child,
                             neripal::core::EvolutionStage::Adult,
                             neripal::core::EvolutionStage::Final}) {
        pet.stage = stage;
        if (!renderAndCheck()) return false;
    }
    return true;
}

bool deviceViewsContainNoDebugLabels() {
    PetState pet;
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;
    view.render(renderer, pet, ui.state());
    for (const auto& text : renderer.texts) {
        if (text.text.find("TIME X") != std::string::npos ||
            text.text.find("F FEED") != std::string::npos ||
            text.text.find("UP/DOWN") != std::string::npos) {
            return false;
        }
    }
    return true;
}

bool careFeedbackAppearsAndExpires() {
    neripal::ui::UiController ui;
    ui.update(0);
    ui.beginCareFeedback(CareAction::Feed, CareResult::Applied, 0);
    if (!ui.state().careFeedbackActive) return false;
    ui.update(899);
    if (!ui.state().careFeedbackActive) return false;
    ui.update(neripal::ui::UiController::kCareFeedbackMillis);
    return !ui.state().careFeedbackActive;
}

bool rejectedFeedbackUsesCareResultLabel() {
    PetState pet;
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;
    ui.beginCareFeedback(CareAction::Train, CareResult::RejectedNoEnergy, 0);
    view.render(renderer, pet, ui.state());
    bool sawTired = false;
    for (const auto& text : renderer.texts) {
        if (text.text == "TIRED") sawTired = true;
    }
    if (!sawTired) return false;

    renderer = FakeRenderer{};
    ui.beginCareFeedback(CareAction::Feed, CareResult::RejectedAsleep, 0);
    view.render(renderer, pet, ui.state());
    for (const auto& text : renderer.texts) {
        if (text.text == "ASLEEP") return true;
    }
    return false;
}

bool homeBannerUsesDerivedMood() {
    PetState pet;
    pet.hunger = 40;
    pet.happiness = 70;
    pet.energy = 80;
    pet.health = 100;
    pet.hygiene = 80;
    pet.sleeping = false;
    pet.stage = neripal::core::EvolutionStage::Baby;

    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;

    const auto shows = [&](const char* label) {
        renderer = FakeRenderer{};
        view.render(renderer, pet, ui.state());
        for (const auto& text : renderer.texts) {
            if (text.text == label) return true;
        }
        return false;
    };

    if (!shows("CALM")) return false;

    pet.happiness = neripal::core::balance::kHappyMoodHappiness + 1;
    if (!shows("HAPPY")) return false;

    pet.happiness = 70;
    pet.hygiene = neripal::core::balance::kHygieneNeglectThreshold;
    if (!shows("DIRTY")) return false;

    pet.hygiene = 80;
    pet.energy = neripal::core::balance::kTiredMoodEnergy;
    if (!shows("TIRED")) return false;

    pet.energy = 80;
    pet.happiness = neripal::core::balance::kAnnoyedMoodHappiness;
    if (!shows("ANNOYED")) return false;

    pet.happiness = 70;
    pet.sleeping = true;
    return shows("RESTING");
}

bool eggBannerIsWaitingRegardlessOfMood() {
    PetState pet;
    pet.stage = neripal::core::EvolutionStage::Egg;
    pet.hygiene = 0;
    pet.energy = 0;
    pet.happiness = 0;
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;
    view.render(renderer, pet, ui.state());
    bool sawWaiting = false;
    for (const auto& text : renderer.texts) {
        if (text.text == "WAITING") sawWaiting = true;
        if (text.text == "DIRTY" || text.text == "TIRED" || text.text == "ANNOYED") return false;
    }
    return sawWaiting;
}

bool overlaysStayInsideLogicalViewport() {
    PetState pet;
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;
    const std::pair<CareAction, CareResult> cases[] = {
        {CareAction::Feed, CareResult::Applied},
        {CareAction::Train, CareResult::Applied},
        {CareAction::Sleep, CareResult::Applied},
        {CareAction::Wake, CareResult::Applied},
        {CareAction::Clean, CareResult::Applied},
        {CareAction::Pet, CareResult::Applied},
        {CareAction::Play, CareResult::Applied},
        {CareAction::Feed, CareResult::RejectedAsleep},
        {CareAction::Train, CareResult::RejectedNoEnergy},
        {CareAction::Sleep, CareResult::RejectedAlreadySleeping},
        {CareAction::Wake, CareResult::RejectedAlreadyAwake},
    };
    for (const auto& [action, result] : cases) {
        ui.beginCareFeedback(action, result, 0);
        renderer = FakeRenderer{};
        view.render(renderer, pet, ui.state());
        if (!renderer.beganFrame || !renderer.endedFrame) return false;
        for (const auto& rect : renderer.rectangles) {
            if (rect.x < 0 || rect.y < 0 || rect.width < 0 || rect.height < 0 ||
                rect.x + rect.width > FakeRenderer::kLogicalWidth ||
                rect.y + rect.height > FakeRenderer::kLogicalHeight) {
                return false;
            }
        }
    }
    return true;
}

bool homePetStaysInsideViewportAtWalkBounds() {
    using neripal::core::Activity;
    PetState pet;
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;
    const int xs[] = {neripal::core::balance::kWalkMinX, neripal::core::balance::kPetHomeX,
                      neripal::core::balance::kWalkMaxX};
    const Activity activities[] = {Activity::Walk,   Activity::Eat,   Activity::Dirty,
                                   Activity::Sleep,  Activity::Nap,   Activity::Annoyed,
                                   Activity::Tired,  Activity::Happy};
    for (int x : xs) {
        for (int facing : {-1, 1}) {
            for (const auto activity : activities) {
                pet.x = x;
                pet.facing = facing;
                pet.activity = activity;
                pet.sleeping = activity == Activity::Sleep || activity == Activity::Nap;
                renderer = FakeRenderer{};
                view.render(renderer, pet, ui.state());
                for (const auto& rect : renderer.rectangles) {
                    if (rect.x < 0 || rect.y < 0 || rect.width < 0 || rect.height < 0 ||
                        rect.x + rect.width > FakeRenderer::kLogicalWidth ||
                        rect.y + rect.height > FakeRenderer::kLogicalHeight) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool eatActivityDrawsFoodWithoutCareOverlay() {
    using neripal::core::Activity;
    PetState pet;
    pet.activity = Activity::Eat;
    pet.x = neripal::core::balance::kPetHomeX;
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;
    view.render(renderer, pet, ui.state());
    FakeRenderer idle;
    PetState calm = pet;
    calm.activity = Activity::Idle;
    view.render(idle, calm, ui.state());
    return renderer.rectangles.size() > idle.rectangles.size();
}

bool dirtyActivityDrawsSpecks() {
    using neripal::core::Activity;
    PetState pet;
    pet.activity = Activity::Dirty;
    pet.hygiene = 0;
    pet.x = neripal::core::balance::kPetHomeX;
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;
    view.render(renderer, pet, ui.state());
    FakeRenderer clean;
    PetState calm = pet;
    calm.activity = Activity::Idle;
    calm.hygiene = 80;
    view.render(clean, calm, ui.state());
    return renderer.rectangles.size() > clean.rectangles.size();
}

bool menuContainsPetAndPlay() {
    PetState pet;
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;
    ui.handleInput(InputAction::Confirm, pet);
    view.render(renderer, pet, ui.state());
    return hasText(renderer, "FEED") && hasText(renderer, "TRAIN") && hasText(renderer, "SLEEP") &&
           hasText(renderer, "CLEAN") && hasText(renderer, "PET") && hasText(renderer, "PLAY") &&
           hasText(renderer, "STATUS") && hasText(renderer, "HOME") &&
           neripal::ui::UiController::kMenuItemCount == 8;
}

bool menuPetInvokesPetAction() {
    PetState pet;
    neripal::ui::UiController ui;
    openMenuAt(ui, pet, neripal::ui::UiController::kMenuPet);
    if (ui.state().menuIndex != neripal::ui::UiController::kMenuPet) return false;
    ui.handleInput(InputAction::Confirm, pet);
    const auto action = ui.takeCareAction();
    return action.has_value() && *action == CareAction::Pet &&
           ui.state().screen == neripal::ui::Screen::Home;
}

bool menuPlayInvokesPlayAction() {
    PetState pet;
    neripal::ui::UiController ui;
    openMenuAt(ui, pet, neripal::ui::UiController::kMenuPlay);
    if (ui.state().menuIndex != neripal::ui::UiController::kMenuPlay) return false;
    ui.handleInput(InputAction::Confirm, pet);
    const auto action = ui.takeCareAction();
    return action.has_value() && *action == CareAction::Play &&
           ui.state().screen == neripal::ui::Screen::Home;
}

bool statusShowsAffectionStimulationStageAndForm() {
    PetState pet;
    pet.stage = neripal::core::EvolutionStage::Child;
    pet.form = neripal::core::FormId::Juvenile;
    pet.affection = 61;
    pet.stimulation = 22;
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;
    openMenuAt(ui, pet, neripal::ui::UiController::kMenuStatus);
    ui.handleInput(InputAction::Confirm, pet);
    view.render(renderer, pet, ui.state());
    return hasText(renderer, "AFE") && hasText(renderer, "STM") && hasText(renderer, "HAP") &&
           hasText(renderer, "HP") && hasText(renderer, "CHILD") && hasText(renderer, "FORM") &&
           hasText(renderer, "JUVENILE") && rectsInside(renderer);
}

bool normalNeedsDoNotShowASignal() {
    PetState pet;
    pet.stage = neripal::core::EvolutionStage::Baby;
    pet.form = neripal::core::FormId::Juvenile;
    pet.hunger = neripal::core::balance::kHungerAttention - 1;
    pet.energy = neripal::core::balance::kLowNeedAttention + 1;
    pet.hygiene = neripal::core::balance::kLowNeedAttention + 1;
    pet.affection = neripal::core::balance::kLowNeedAttention + 1;
    pet.stimulation = neripal::core::balance::kLowNeedAttention + 1;
    if (neripal::ui::needSignals(pet).count != 0) return false;
    if (neripal::ui::urgentSoundRequested(pet)) return false;
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer renderer;
    view.render(renderer, pet, ui.state());
    return !hasText(renderer, "HUN") && !hasText(renderer, "NRG") && !hasText(renderer, "HYG") &&
           !hasText(renderer, "AFE") && !hasText(renderer, "STM") && !hasText(renderer, "!");
}

bool attentionAndUrgentShareTheNeedSymbol() {
    using neripal::core::Need;
    using neripal::core::NeedLevel;
    using neripal::ui::needSignals;
    using neripal::ui::needSymbol;
    PetState attention;
    attention.stage = neripal::core::EvolutionStage::Baby;
    attention.hunger = neripal::core::balance::kHungerAttention;
    PetState urgent = attention;
    urgent.hunger = neripal::core::balance::kHungerUrgent;
    const auto attentionList = needSignals(attention);
    const auto urgentList = needSignals(urgent);
    if (attentionList.count != 1 || urgentList.count != 1) return false;
    if (attentionList.items[0].need != Need::Hunger || attentionList.items[0].level != NeedLevel::Attention) {
        return false;
    }
    if (urgentList.items[0].need != Need::Hunger || urgentList.items[0].level != NeedLevel::Urgent) {
        return false;
    }
    if (needSymbol(attentionList.items[0].need) != needSymbol(urgentList.items[0].need)) return false;
    if (std::string_view(needSymbol(Need::Hunger)) != "HUN") return false;
    if (attentionList.urgentSound || !urgentList.urgentSound) return false;
    if (neripal::ui::urgentSoundRequested(attention)) return false;
    if (!neripal::ui::urgentSoundRequested(urgent)) return false;

    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer attentionView;
    FakeRenderer urgentView;
    view.render(attentionView, attention, ui.state());
    view.render(urgentView, urgent, ui.state());
    if (!hasText(attentionView, "HUN") || hasText(attentionView, "!")) return false;
    if (!hasText(urgentView, "HUN") || !hasText(urgentView, "!")) return false;
    return urgentView.rectangles.size() > attentionView.rectangles.size() && rectsInside(urgentView);
}

bool eachNeedUsesItsOwnSignal() {
    using neripal::core::Need;
    using neripal::core::balance::kHungerAttention;
    using neripal::core::balance::kLowNeedAttention;
    const std::pair<Need, const char*> expected[] = {
        {Need::Hunger, "HUN"},
        {Need::Energy, "NRG"},
        {Need::Hygiene, "HYG"},
        {Need::Affection, "AFE"},
        {Need::Stimulation, "STM"},
    };
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    for (const auto& [need, symbol] : expected) {
        if (std::string_view(neripal::ui::needSymbol(need)) != symbol) return false;
        PetState pet;
        pet.stage = neripal::core::EvolutionStage::Baby;
        pet.form = neripal::core::FormId::Juvenile;
        if (need == Need::Hunger) {
            pet.hunger = kHungerAttention;
        } else if (need == Need::Energy) {
            pet.energy = kLowNeedAttention;
        } else if (need == Need::Hygiene) {
            pet.hygiene = kLowNeedAttention;
        } else if (need == Need::Affection) {
            pet.affection = kLowNeedAttention;
        } else {
            pet.stimulation = kLowNeedAttention;
        }
        const auto signals = neripal::ui::needSignals(pet);
        if (signals.count != 1 || signals.items[0].need != need) return false;
        if (signals.items[0].level != neripal::core::NeedLevel::Attention) return false;
        FakeRenderer renderer;
        view.render(renderer, pet, ui.state());
        if (!hasText(renderer, symbol) || hasText(renderer, "!")) return false;
        for (const auto& [other, otherSymbol] : expected) {
            if (other != need && hasText(renderer, otherSymbol)) return false;
        }
        if (!rectsInside(renderer)) return false;
    }
    return true;
}

bool evolutionNoticeIsShownThenConfirmedInOrder() {
    FakeClock clock;
    FakeRandom rng;
    neripal::core::Pet pet(clock, rng);
    neripal::core::PetSnapshot snap;
    snap.stage = neripal::core::EvolutionStage::Child;
    snap.form = neripal::core::FormId::Juvenile;
    snap.noticeCount = 2;
    snap.notices[0].from = neripal::core::EvolutionStage::Egg;
    snap.notices[0].to = neripal::core::EvolutionStage::Baby;
    snap.notices[0].form = neripal::core::FormId::Juvenile;
    snap.notices[1].from = neripal::core::EvolutionStage::Baby;
    snap.notices[1].to = neripal::core::EvolutionStage::Child;
    snap.notices[1].form = neripal::core::FormId::Juvenile;
    pet.restoreSnapshot(snap);

    neripal::ui::UiController ui;
    neripal::ui::PetView view;
    syncEvolutionNotice(ui, pet);
    if (!ui.showingEvolutionNotice() || pet.pendingEvolutionNotices() != 2) return false;
    if (ui.takeEvolutionConfirm()) return false;

    FakeRenderer first;
    view.render(first, pet.state(), ui.state());
    if (!hasText(first, "EVOLVED") || !hasText(first, "EGG > BABY") || !hasText(first, "JUVENILE")) {
        return false;
    }
    if (!rectsInside(first)) return false;

    ui.handleInput(InputAction::Confirm, pet.state());
    syncEvolutionNotice(ui, pet);
    if (pet.pendingEvolutionNotices() != 1) return false;
    if (pet.state().stage != neripal::core::EvolutionStage::Child) return false;
    if (pet.state().form != neripal::core::FormId::Juvenile) return false;

    FakeRenderer second;
    view.render(second, pet.state(), ui.state());
    if (!hasText(second, "BABY > CHILD") || !hasText(second, "JUVENILE")) return false;

    ui.handleInput(InputAction::Confirm, pet.state());
    syncEvolutionNotice(ui, pet);
    pet.update();
    return pet.pendingEvolutionNotices() == 0 && !ui.showingEvolutionNotice() &&
           pet.state().stage == neripal::core::EvolutionStage::Child &&
           pet.state().form == neripal::core::FormId::Juvenile && rng.remaining() == 0;
}

bool confirmingNoticeDoesNotChangeStageOrForm() {
    FakeClock clock;
    FakeRandom rng(std::vector<std::uint32_t>{1});
    neripal::core::Pet pet(clock, rng);
    neripal::core::PetSnapshot snap;
    snap.stage = neripal::core::EvolutionStage::Adult;
    snap.form = neripal::core::FormId::AdultB;
    snap.ageMillis = neripal::core::evolution::kAdultAgeMs;
    snap.noticeCount = 1;
    snap.notices[0].from = neripal::core::EvolutionStage::Child;
    snap.notices[0].to = neripal::core::EvolutionStage::Adult;
    snap.notices[0].form = neripal::core::FormId::AdultB;
    pet.restoreSnapshot(snap);

    neripal::ui::UiController ui;
    syncEvolutionNotice(ui, pet);
    ui.handleInput(InputAction::Next, pet.state());
    if (!ui.showingEvolutionNotice() || ui.takeEvolutionConfirm()) return false;
    ui.handleInput(InputAction::Confirm, pet.state());
    if (!ui.takeEvolutionConfirm()) return false;
    if (!pet.confirmEvolutionNotice()) return false;
    pet.update();
    return pet.pendingEvolutionNotices() == 0 &&
           pet.state().stage == neripal::core::EvolutionStage::Adult &&
           pet.state().form == neripal::core::FormId::AdultB && rng.remaining() == 1;
}

bool finalUsesTheAdultPlaceholder() {
    PetState adult;
    adult.stage = neripal::core::EvolutionStage::Adult;
    adult.form = neripal::core::FormId::AdultC;
    adult.x = neripal::core::balance::kPetHomeX;
    PetState finalForm = adult;
    finalForm.stage = neripal::core::EvolutionStage::Final;
    PetState baby = adult;
    baby.stage = neripal::core::EvolutionStage::Baby;
    baby.form = neripal::core::FormId::Juvenile;

    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    FakeRenderer adultView;
    FakeRenderer finalView;
    FakeRenderer babyView;
    view.render(adultView, adult, ui.state());
    view.render(finalView, finalForm, ui.state());
    view.render(babyView, baby, ui.state());
    if (adultView.rectangles.size() != finalView.rectangles.size()) return false;
    for (std::size_t i = 0; i < adultView.rectangles.size(); ++i) {
        const auto& left = adultView.rectangles[i];
        const auto& right = finalView.rectangles[i];
        if (left.x != right.x || left.y != right.y || left.width != right.width ||
            left.height != right.height) {
            return false;
        }
    }
    return babyView.rectangles.size() != adultView.rectangles.size();
}

bool stageAndFormAreCoherent(neripal::core::EvolutionStage stage, neripal::core::FormId form) {
    using neripal::core::EvolutionStage;
    using neripal::core::FormId;
    switch (stage) {
        case EvolutionStage::Egg:
            return form == FormId::None;
        case EvolutionStage::Baby:
        case EvolutionStage::Child:
            return form == FormId::Juvenile;
        case EvolutionStage::Adult:
        case EvolutionStage::Final:
            return form == FormId::AdultA || form == FormId::AdultB || form == FormId::AdultC ||
                   form == FormId::AdultSecret;
    }
    return false;
}

bool debugForceEvolutionKeepsStageAndFormCoherent() {
    FakeClock clock;
    FakeRandom rng;
    neripal::core::Pet pet(clock, rng);
    FakeSaveStorage storage;
    FakeWallClock wall;
    FakeClock session;
    neripal::persist::SaveSession saves(pet, storage, wall, session);
    neripal::desktop::ScaledClock scaled;
    neripal::simulator::DebugController debug(pet, scaled, saves);

    if (pet.state().stage != neripal::core::EvolutionStage::Egg) return false;
    for (int step = 0; step < 5; ++step) {
        debug.forceEvolution();
        if (!stageAndFormAreCoherent(pet.state().stage, pet.state().form)) return false;
    }
    if (pet.state().stage != neripal::core::EvolutionStage::Egg ||
        pet.state().form != neripal::core::FormId::None) {
        return false;
    }

    auto state = pet.state();
    state.stage = neripal::core::EvolutionStage::Adult;
    state.form = neripal::core::FormId::AdultSecret;
    pet.restore(state);
    debug.forceEvolution();
    if (pet.state().stage != neripal::core::EvolutionStage::Final ||
        pet.state().form != neripal::core::FormId::AdultSecret) {
        return false;
    }

    state = pet.state();
    state.stage = neripal::core::EvolutionStage::Adult;
    state.form = neripal::core::FormId::Juvenile;
    pet.restore(state);
    debug.forceEvolution();
    if (pet.state().stage != neripal::core::EvolutionStage::Final ||
        pet.state().form != neripal::core::FormId::AdultC) {
        return false;
    }

    state = pet.state();
    state.stage = neripal::core::EvolutionStage::Child;
    state.form = neripal::core::FormId::Juvenile;
    pet.restore(state);
    debug.forceEvolution();
    if (pet.state().stage != neripal::core::EvolutionStage::Adult ||
        pet.state().form != neripal::core::FormId::AdultC) {
        return false;
    }

    debug.adjustStat(5, -100);
    const auto signals = neripal::ui::needSignals(pet.state());
    bool affectionUrgent = false;
    for (std::uint8_t i = 0; i < signals.count; ++i) {
        if (signals.items[i].need == neripal::core::Need::Affection &&
            signals.items[i].level == neripal::core::NeedLevel::Urgent) {
            affectionUrgent = true;
        }
    }
    return affectionUrgent && rng.remaining() == 0;
}

bool textsFit(const FakeRenderer& renderer, std::string_view scene) {
    for (const auto& command : renderer.texts) {
        std::string detail;
        if (textCommandFits(command, detail)) continue;
        std::cerr << "textFitsViewport " << scene << " " << detail << '\n';
        return false;
    }
    return true;
}

bool renderTextsFit(neripal::ui::PetView& view, const PetState& pet,
                    const neripal::ui::UiState& uiState, std::string_view scene) {
    FakeRenderer renderer;
    view.render(renderer, pet, uiState);
    return textsFit(renderer, scene);
}

bool textFitsViewport() {
    FakeRenderer::TextCommand collapsed{230, 0, 0, "AB"};
    std::string collapsedDetail;
    if (textCommandFits(collapsed, collapsedDetail)) {
        std::cerr << "textFitsViewport scale<1 was not treated as 1\n";
        return false;
    }

    using neripal::core::EvolutionStage;
    using neripal::core::FormId;
    using neripal::core::balance::kHungerAttention;
    using neripal::core::balance::kHungerUrgent;
    using neripal::core::balance::kLowNeedAttention;
    using neripal::core::balance::kLowNeedUrgent;
    neripal::ui::PetView view;

    const auto formForStage = [](EvolutionStage stage) {
        switch (stage) {
            case EvolutionStage::Egg: return FormId::None;
            case EvolutionStage::Baby:
            case EvolutionStage::Child: return FormId::Juvenile;
            case EvolutionStage::Adult:
            case EvolutionStage::Final: return FormId::AdultC;
        }
        return FormId::None;
    };
    const EvolutionStage stages[] = {
        EvolutionStage::Egg, EvolutionStage::Baby, EvolutionStage::Child,
        EvolutionStage::Adult, EvolutionStage::Final};
    const int needLevels[] = {0, 1, 2};

    for (const EvolutionStage stage : stages) {
        for (const int level : needLevels) {
            for (const bool sleeping : {false, true}) {
                PetState pet;
                pet.stage = stage;
                pet.form = formForStage(stage);
                pet.sleeping = sleeping;
                if (level == 0) {
                    pet.hunger = 35;
                    pet.energy = 80;
                    pet.hygiene = 80;
                    pet.affection = 70;
                    pet.stimulation = 70;
                } else if (level == 1) {
                    pet.hunger = kHungerAttention;
                    pet.energy = kLowNeedAttention;
                    pet.hygiene = kLowNeedAttention;
                    pet.affection = kLowNeedAttention;
                    pet.stimulation = kLowNeedAttention;
                } else {
                    pet.hunger = kHungerUrgent;
                    pet.energy = kLowNeedUrgent;
                    pet.hygiene = kLowNeedUrgent;
                    pet.affection = kLowNeedUrgent;
                    pet.stimulation = kLowNeedUrgent;
                }
                neripal::ui::UiController ui;
                const char* need = level == 0 ? "normal" : level == 1 ? "attention" : "urgent";
                const std::string scene = std::string("home stage=") + std::to_string(static_cast<int>(stage)) +
                                          " " + need + (sleeping ? " asleep" : " awake");
                if (!renderTextsFit(view, pet, ui.state(), scene)) return false;
            }
        }
    }

    PetState edge;
    edge.stage = EvolutionStage::Baby;
    edge.form = FormId::Juvenile;
    edge.sleeping = true;
    edge.x = 0;
    neripal::ui::UiController home;
    if (!renderTextsFit(view, edge, home.state(), "home asleep x=0")) return false;
    edge.x = FakeRenderer::kLogicalWidth;
    if (!renderTextsFit(view, edge, home.state(), "home asleep x=240")) return false;

    PetState menuPet;
    menuPet.stage = EvolutionStage::Baby;
    menuPet.form = FormId::Juvenile;
    neripal::ui::UiController menu;
    openMenuAt(menu, menuPet, 0);
    for (int index = 0; index < neripal::ui::UiController::kMenuItemCount; ++index) {
        if (menu.state().menuIndex != index) return false;
        for (const bool sleeping : {false, true}) {
            menuPet.sleeping = sleeping;
            const std::string scene = std::string("menu index=") + std::to_string(index) +
                                      (sleeping ? " asleep" : " awake");
            if (!renderTextsFit(view, menuPet, menu.state(), scene)) return false;
        }
        menu.handleInput(InputAction::Next, menuPet);
    }

    const std::pair<EvolutionStage, FormId> statusForms[] = {
        {EvolutionStage::Egg, FormId::None},
        {EvolutionStage::Baby, FormId::Juvenile},
        {EvolutionStage::Child, FormId::Juvenile},
        {EvolutionStage::Adult, FormId::AdultA},
        {EvolutionStage::Adult, FormId::AdultB},
        {EvolutionStage::Adult, FormId::AdultC},
        {EvolutionStage::Adult, FormId::AdultSecret},
        {EvolutionStage::Final, FormId::AdultA},
        {EvolutionStage::Final, FormId::AdultB},
        {EvolutionStage::Final, FormId::AdultC},
        {EvolutionStage::Final, FormId::AdultSecret},
    };
    for (const auto& [stage, form] : statusForms) {
        for (const bool sleeping : {false, true}) {
            PetState pet;
            pet.stage = stage;
            pet.form = form;
            pet.sleeping = sleeping;
            pet.hunger = 100;
            pet.energy = 0;
            pet.hygiene = 50;
            pet.affection = 100;
            pet.stimulation = 0;
            pet.happiness = 100;
            pet.health = 0;
            pet.ageMillis = 72ULL * 60 * 60 * 1000;
            neripal::ui::UiController ui;
            openMenuAt(ui, pet, neripal::ui::UiController::kMenuStatus);
            ui.handleInput(InputAction::Confirm, pet);
            if (ui.state().screen != neripal::ui::Screen::Status) return false;
            const std::string scene = std::string("status stage=") + std::to_string(static_cast<int>(stage)) +
                                      " form=" + std::to_string(static_cast<int>(form)) +
                                      (sleeping ? " asleep" : " awake");
            if (!renderTextsFit(view, pet, ui.state(), scene)) return false;
        }
    }

    const neripal::core::EvolutionNotice notices[] = {
        {EvolutionStage::Egg, EvolutionStage::Baby, FormId::Juvenile},
        {EvolutionStage::Baby, EvolutionStage::Child, FormId::Juvenile},
        {EvolutionStage::Child, EvolutionStage::Adult, FormId::AdultA},
        {EvolutionStage::Child, EvolutionStage::Adult, FormId::AdultB},
        {EvolutionStage::Child, EvolutionStage::Adult, FormId::AdultC},
        {EvolutionStage::Child, EvolutionStage::Adult, FormId::AdultSecret},
        {EvolutionStage::Adult, EvolutionStage::Final, FormId::AdultA},
        {EvolutionStage::Adult, EvolutionStage::Final, FormId::AdultB},
        {EvolutionStage::Adult, EvolutionStage::Final, FormId::AdultC},
        {EvolutionStage::Adult, EvolutionStage::Final, FormId::AdultSecret},
    };
    PetState noticePet;
    noticePet.stage = EvolutionStage::Child;
    noticePet.form = FormId::Juvenile;
    for (const auto& notice : notices) {
        neripal::ui::UiController ui;
        ui.presentEvolutionNotice(notice);
        if (!ui.showingEvolutionNotice()) return false;
        const std::string scene = std::string("notice ") + std::to_string(static_cast<int>(notice.from)) +
                                  ">" + std::to_string(static_cast<int>(notice.to)) +
                                  " form=" + std::to_string(static_cast<int>(notice.form));
        if (!renderTextsFit(view, noticePet, ui.state(), scene)) return false;
    }
    return true;
}

bool rgb565Conversion() {
    using neripal::platform::toRgb565;
    const std::pair<Color, std::uint16_t> cases[] = {
        {0x00000000u, 0x0000u},
        {0x00FFFFFFu, 0xFFFFu},
        {0x00FF0000u, 0xF800u},
        {0x0000FF00u, 0x07E0u},
        {0x000000FFu, 0x001Fu},
        {0x001A2440u, 0x1928u},
        {0x002F3857u, 0x29CAu},
        {0x0086C9D1u, 0x865Au},
        {0x00D95850u, 0xDACAu},
        {0x00F6C75Du, 0xF62Bu},
        {0x005B79BCu, 0x5BD7u},
        {0x00FFF0C9u, 0xFF99u},
    };
    for (const auto& [color, expected] : cases) {
        const std::uint16_t actual = toRgb565(color);
        if (actual == expected) continue;
        std::cerr << std::hex << "toRgb565(0x" << color << ")=0x" << actual << " expected 0x"
                  << expected << std::dec << '\n';
        return false;
    }
    if (toRgb565(0x00FF0000u) == toRgb565(0x000000FFu)) return false;
    if (toRgb565(0x0000FF00u) == 0x03E0u) return false;
    return true;
}
}

int main() {
    const std::vector<TestCase> tests{
        {"menu navigation wraps eight items", menuNavigationWrapsEightItems},
        {"menu contains pet and play", menuContainsPetAndPlay},
        {"menu pet invokes pet action", menuPetInvokesPetAction},
        {"menu play invokes play action", menuPlayInvokesPlayAction},
        {"status returns to menu", statusReturnsToMenu},
        {"feed enqueues care action and returns home", feedEnqueuesCareActionAndReturnsHome},
        {"sleep menu selects wake when sleeping", sleepMenuSelectsWakeWhenSleeping},
        {"sleep menu selects sleep when awake", sleepMenuSelectsSleepWhenAwake},
        {"status does not enqueue care action", statusDoesNotEnqueueCareAction},
        {"menu selection eases out over 240 ms", menuSelectionEasesOutOver240ms},
        {"footer shows only accepted actions", footerShowsOnlyAcceptedActions},
        {"idle animation uses controlled time", idleAnimationUsesControlledTime},
        {"status shows hygiene bar", statusShowsHygieneBar},
        {"every screen stays inside logical viewport", everyScreenStaysInsideLogicalViewport},
        {"device views contain no debug labels", deviceViewsContainNoDebugLabels},
        {"care feedback appears and expires", careFeedbackAppearsAndExpires},
        {"rejected feedback uses care result label", rejectedFeedbackUsesCareResultLabel},
        {"home banner uses derived mood", homeBannerUsesDerivedMood},
        {"egg banner is waiting regardless of mood", eggBannerIsWaitingRegardlessOfMood},
        {"overlays stay inside logical viewport", overlaysStayInsideLogicalViewport},
        {"home pet stays inside viewport at walk bounds", homePetStaysInsideViewportAtWalkBounds},
        {"eat activity draws food without care overlay", eatActivityDrawsFoodWithoutCareOverlay},
        {"dirty activity draws specks", dirtyActivityDrawsSpecks},
        {"status shows affection stimulation stage and form", statusShowsAffectionStimulationStageAndForm},
        {"normal needs do not show a signal", normalNeedsDoNotShowASignal},
        {"attention and urgent share the need symbol", attentionAndUrgentShareTheNeedSymbol},
        {"each need uses its own signal", eachNeedUsesItsOwnSignal},
        {"evolution notice is shown then confirmed in order", evolutionNoticeIsShownThenConfirmedInOrder},
        {"confirming notice does not change stage or form", confirmingNoticeDoesNotChangeStageOrForm},
        {"final uses the adult placeholder", finalUsesTheAdultPlaceholder},
        {"debug force evolution keeps stage and form coherent", debugForceEvolutionKeepsStageAndFormCoherent},
        {"text fits viewport", textFitsViewport},
        {"rgb565 conversion", rgb565Conversion},
    };

    int failures = 0;
    for (const auto& test : tests) {
        const bool passed = test.run();
        std::cout << (passed ? "[PASS] " : "[FAIL] ") << test.name << '\n';
        failures += passed ? 0 : 1;
    }
    std::cout << tests.size() - failures << '/' << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
