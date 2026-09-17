#include "neripal/core/Care.hpp"
#include "neripal/core/PetState.hpp"
#include "neripal/platform/IRenderer.hpp"
#include "neripal/ui/PetView.hpp"
#include "neripal/ui/UiController.hpp"

#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
using neripal::core::CareAction;
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
