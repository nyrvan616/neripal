#include "neripal/core/Activity.hpp"
#include "neripal/core/Balance.hpp"
#include "neripal/core/Care.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/core/PetSnapshot.hpp"
#include "neripal/core/PetState.hpp"
#include "neripal/persist/SaveSession.hpp"
#include "neripal/platform/IRenderer.hpp"
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

bool hasText(const FakeRenderer& renderer, std::string_view text) {
    for (const auto& item : renderer.texts) {
        if (item == text) return true;
    }
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
