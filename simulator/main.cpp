#include "DebugController.hpp"
#include "neripal/Version.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/ui/PetView.hpp"
#include "platform/desktop/DesktopPlatform.hpp"
#include "platform/desktop/ScaledClock.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <string_view>

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
                case VK_TAB: selectedStat = (selectedStat + 1) % 4; break;
                case VK_UP: debug.adjustStat(selectedStat, 5); break;
                case VK_DOWN: debug.adjustStat(selectedStat, -5); break;
                case VK_HOME: debug.adjustStat(selectedStat, 100); break;
                case VK_END: debug.adjustStat(selectedStat, -100); break;
                case VK_ESCAPE: PostQuitMessage(0); break;
                default: break;
            }
        }
        pet.update();
        view.render(platform, pet.state(), debug.timeScale(), selectedStat);
        platform.waitForNextFrame();
    }
    return 0;
}
