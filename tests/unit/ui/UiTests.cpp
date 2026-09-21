#include "neripal/core/Activity.hpp"
#include "neripal/core/Balance.hpp"
#include "neripal/core/Care.hpp"
#include "neripal/core/PetState.hpp"
#include "neripal/platform/IRenderer.hpp"
#include "neripal/ui/PetView.hpp"
#include "neripal/ui/UiController.hpp"

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

    void beginFrame(Color) override { beganFrame = true; }
    void fillRect(int x, int y, int width, int height, Color) override {
        rectangles.push_back({x, y, width, height});
    }
    void drawRect(int x, int y, int width, int height, Color) override {
        rectangles.push_back({x, y, width, height});
    }
    void drawText(int, int, std::string_view text, Color, int) override {
        texts.emplace_back(text);
    }
    void endFrame() override { endedFrame = true; }

    bool beganFrame = false;
    bool endedFrame = false;
    std::vector<Rect> rectangles;
    std::vector<std::string> texts;
};

bool menuNavigationWrapsSixItems() {
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
    ui.handleInput(InputAction::Confirm, pet);
    for (int i = 0; i < 4; ++i) {
        ui.handleInput(InputAction::Next, pet);
    }
    if (ui.state().menuIndex != 4) return false;
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
    ui.handleInput(InputAction::Confirm, pet);
    for (int i = 0; i < 4; ++i) {
        ui.handleInput(InputAction::Next, pet);
    }
    ui.handleInput(InputAction::Confirm, pet);
    return !ui.takeCareAction().has_value();
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
    ui.handleInput(InputAction::Confirm, pet);
    for (int i = 0; i < 4; ++i) {
        ui.handleInput(InputAction::Next, pet);
    }
    ui.handleInput(InputAction::Confirm, pet);
    view.render(renderer, pet, ui.state());
    for (const auto& text : renderer.texts) {
        if (text == "HYG") return true;
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
                             neripal::core::EvolutionStage::Adult}) {
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
                             neripal::core::EvolutionStage::Adult}) {
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
        if (text.find("TIME X") != std::string::npos ||
            text.find("F FEED") != std::string::npos ||
            text.find("UP/DOWN") != std::string::npos) {
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
        if (text == "TIRED") sawTired = true;
    }
    if (!sawTired) return false;

    renderer = FakeRenderer{};
    ui.beginCareFeedback(CareAction::Feed, CareResult::RejectedAsleep, 0);
    view.render(renderer, pet, ui.state());
    for (const auto& text : renderer.texts) {
        if (text == "ASLEEP") return true;
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
            if (text == label) return true;
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
        if (text == "WAITING") sawWaiting = true;
        if (text == "DIRTY" || text == "TIRED" || text == "ANNOYED") return false;
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
}

int main() {
    const std::vector<TestCase> tests{
        {"menu navigation wraps six items", menuNavigationWrapsSixItems},
        {"status returns to menu", statusReturnsToMenu},
        {"feed enqueues care action and returns home", feedEnqueuesCareActionAndReturnsHome},
        {"sleep menu selects wake when sleeping", sleepMenuSelectsWakeWhenSleeping},
        {"sleep menu selects sleep when awake", sleepMenuSelectsSleepWhenAwake},
        {"status does not enqueue care action", statusDoesNotEnqueueCareAction},
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
