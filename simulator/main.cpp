#include "DebugController.hpp"
#include "neripal/Version.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/core/XorShift32.hpp"
#include "neripal/persist/SaveSession.hpp"
#include "neripal/core/EvolutionNotice.hpp"
#include "neripal/ui/NeedSignals.hpp"
#include "neripal/ui/PetView.hpp"
#include "neripal/ui/UiController.hpp"
#include "platform/desktop/DesktopPlatform.hpp"
#include "platform/desktop/FileSaveStorage.hpp"
#include "platform/desktop/ScaledClock.hpp"
#include "platform/desktop/SessionClock.hpp"
#include "platform/desktop/SystemWallClock.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::filesystem::path executableDirectory() {
    wchar_t buffer[MAX_PATH]{};
    const auto length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buffer).parent_path();
}

const char* bootLabel(neripal::persist::BootResult result) {
    switch (result) {
        case neripal::persist::BootResult::Restored:
            return "RESTORED";
        case neripal::persist::BootResult::RestoredFromBackup:
            return "BACKUP";
        case neripal::persist::BootResult::Fresh:
        default:
            return "FRESH";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << neripal::version::kDisplayName << '\n';
    if (argc > 1 && std::string_view(argv[1]) == "--version") {
        return 0;
    }

    std::cout << "Desktop simulator | logical display 240x240\n"
              << "Press Esc to exit. Controls are shown in the game window.\n"
              << std::flush;

    HINSTANCE instance = GetModuleHandleW(nullptr);
    neripal::desktop::ScaledClock clock;
    neripal::desktop::SessionClock sessionClock;
    neripal::desktop::SystemWallClock wallClock;
    const auto saveDir = executableDirectory();
    neripal::desktop::FileSaveStorage storage(saveDir);
    // Development default chosen by this composition root, not by Core.
    constexpr std::uint32_t kSimulatorRngSeed = 0x4E455249u;  // 'NERI'
    neripal::core::XorShift32 rng(kSimulatorRngSeed);
    neripal::core::Pet pet(clock, rng);
    neripal::persist::SaveSession saves(pet, storage, wallClock, sessionClock);
    const auto boot = saves.boot();
    std::cout << "Save directory: " << saveDir.string() << '\n'
              << "Boot: " << bootLabel(boot) << '\n'
              << std::flush;

    neripal::desktop::DesktopPlatform platform(instance);
    neripal::simulator::DebugController debug(pet, clock, saves);
    neripal::ui::PetView view;
    neripal::ui::UiController ui;
    int selectedStat = 0;

    if (!platform.valid()) return 1;

    while (platform.running()) {
        platform.pumpEvents();
        while (const auto key = platform.pollKey()) {
            switch (*key) {
                case 'F': debug.feed(); break;
                case 'T': debug.train(); break;
                case 'S': pet.state().sleeping ? debug.wake() : debug.sleep(); break;
                case 'W': debug.wake(); break;
                case 'L': debug.clean(); break;
                case 'P': debug.pet(); break;
                case 'Y': debug.play(); break;
                case 'R': debug.reset(); break;
                case '1': debug.setTimeScale(1); break;
                case '2': debug.setTimeScale(10); break;
                case '3': debug.setTimeScale(100); break;
                case '4': debug.setTimeScale(1000); break;
                case 'A': debug.advanceMinutes(60); break;
                case 'E': debug.forceEvolution(); break;
                case VK_TAB:
                    selectedStat = (selectedStat + 1) %
                                   neripal::simulator::DebugController::kDebugStatCount;
                    break;
                case VK_UP: debug.adjustStat(selectedStat, 5); break;
                case VK_DOWN: debug.adjustStat(selectedStat, -5); break;
                case VK_HOME: debug.adjustStat(selectedStat, 100); break;
                case VK_END: debug.adjustStat(selectedStat, -100); break;
                case VK_ESCAPE: PostQuitMessage(0); break;
                default: break;
            }
        }
        while (const auto action = platform.pollAction()) {
            ui.handleInput(*action, pet.state());
        }
        if (ui.takeEvolutionConfirm()) {
            pet.confirmEvolutionNotice();
        }
        if (!ui.showingEvolutionNotice()) {
            neripal::core::EvolutionNotice notice;
            if (pet.peekEvolutionNotice(notice)) {
                ui.presentEvolutionNotice(notice);
            }
        }
        if (const auto care = ui.takeCareAction()) {
            const auto result = pet.apply(*care);
            saves.noteCareResult(result);
            ui.beginCareFeedback(*care, result, clock.nowMillis());
        }
        pet.update();
        ui.update(clock.nowMillis());
        saves.tick();

        static constexpr std::array<std::string_view, 7> kStatNames{
            "HUNGER", "HAPPINESS", "ENERGY", "HEALTH", "HYGIENE", "AFFECTION", "STIMULATION"};
        const auto signals = neripal::ui::needSignals(pet.state());
        std::string needs = "NEEDS";
        for (std::uint8_t i = 0; i < signals.count; ++i) {
            needs += " ";
            needs += neripal::ui::needSymbol(signals.items[i].need);
            if (signals.items[i].level == neripal::core::NeedLevel::Urgent) needs += "!";
        }
        if (signals.count == 0) needs += " ok";
        neripal::core::EvolutionNotice pending{};
        const bool hasNotice = pet.peekEvolutionNotice(pending);
        std::string noticeLine = "NOTICE " + std::to_string(pet.pendingEvolutionNotices());
        if (hasNotice) {
            noticeLine += " queued";
        }
        const bool urgentSoundCue = neripal::ui::urgentSoundRequested(pet.state());
        platform.setDebugLines({
            "DEVICE CONTROLS",
            "Z/RIGHT  next",
            "X/ENTER  select",
            "C/BACKSPACE  back",
            "Esc  quit",
            "",
            "SAVE " + std::string(bootLabel(saves.bootResult())),
            std::string(saves.dirty() ? "DIRTY yes" : "DIRTY no"),
            "TIME x" + std::to_string(debug.timeScale()),
            "EDIT " + std::string(kStatNames[static_cast<std::size_t>(selectedStat)]),
            "Up/Down  +/-5",
            "Home/End  max/min",
            "",
            needs,
            noticeLine,
            std::string(urgentSoundCue ? "SOUND cue" : "SOUND off"),
            "F feed   T train",
            "S sleep  W wake",
            "L clean  P pet",
            "Y play   R reset",
            "A +1 hour",
            "E force stage/form",
            "1-4 time scale",
        });
        view.render(platform, pet.state(), ui.state());
        platform.waitForNextFrame();
    }
    if (!saves.saveNow()) {
        std::cout << "Save on exit failed\n" << std::flush;
    }
    return 0;
}
