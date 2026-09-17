#include "DebugController.hpp"
#include "neripal/Version.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/ui/PetView.hpp"
#include "neripal/ui/UiController.hpp"
#include "platform/desktop/DesktopPlatform.hpp"
#include "platform/desktop/ScaledClock.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <array>
#include <string>
#include <string_view>
#include <vector>

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
    neripal::core::Pet pet(clock);
    neripal::desktop::DesktopPlatform platform(instance);
    neripal::simulator::DebugController debug(pet, clock);
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
                case 'R': debug.reset(); break;
                case '1': debug.setTimeScale(1); break;
                case '2': debug.setTimeScale(10); break;
                case '3': debug.setTimeScale(100); break;
                case '4': debug.setTimeScale(1000); break;
                case 'A': debug.advanceMinutes(60); break;
                case 'E': debug.forceEvolution(); break;
                case VK_TAB: selectedStat = (selectedStat + 1) % 4; break;
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
        pet.update();
        ui.update(clock.nowMillis());

        static constexpr std::array<std::string_view, 4> kStatNames{
            "HUNGER", "HAPPINESS", "ENERGY", "HEALTH"};
        platform.setDebugLines({
            "DEVICE CONTROLS",
            "Z/RIGHT  next",
            "X/ENTER  select",
            "C/BACKSPACE  back",
            "Esc  quit",
            "",
            "TIME x" + std::to_string(debug.timeScale()),
            "EDIT " + std::string(kStatNames[static_cast<std::size_t>(selectedStat)]),
            "Up/Down  +/-5",
            "Home/End  max/min",
            "",
            "F feed   T train",
            "S sleep  W wake",
            "R reset  A +1 hour",
            "E force evolution",
            "1-4 time scale",
        });
        view.render(platform, pet.state(), ui.state());
        platform.waitForNextFrame();
    }
    return 0;
}
