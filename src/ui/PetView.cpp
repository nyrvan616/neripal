#include "neripal/ui/PetView.hpp"

#include "neripal/Version.hpp"

#include <algorithm>
#include <cstdio>

namespace neripal::ui {
namespace {
constexpr platform::Color kInk = 0x0023322A;
constexpr platform::Color kPaper = 0x00D9E6C3;
constexpr platform::Color kPanel = 0x00B8D49D;
constexpr platform::Color kAccent = 0x004D8B65;
constexpr platform::Color kAlert = 0x00B84A3A;
constexpr platform::Color kSleep = 0x004957A8;
}

void PetView::drawStat(platform::IRenderer& r, int y, const char* label,
                       int value, platform::Color color, bool selected) {
    if (selected) {
        r.fillRect(7, y - 2, 226, 18, kPanel);
        r.drawRect(7, y - 2, 226, 18, kAccent);
    }
    r.drawText(12, y, label, kInk);
    r.drawRect(72, y, 124, 10, kInk);
    const int fill = std::clamp(value, 0, 100) * 120 / 100;
    r.fillRect(74, y + 2, fill, 6, color);
    char buffer[8]{};
    std::snprintf(buffer, sizeof(buffer), "%3d", value);
    r.drawText(204, y, buffer, kInk);
}

void PetView::render(platform::IRenderer& r, const core::PetState& state,
                     int timeScale, int selectedStat) const {
    r.beginFrame(kPaper);
    r.fillRect(0, 0, 240, 22, kInk);
    r.drawText(8, 7, version::kScreenLabel, kPaper, 1);
    char speed[16]{};
    std::snprintf(speed, sizeof(speed), "TIME X%d", timeScale);
    r.drawText(163, 7, speed, kPaper);

    // Dependency-free placeholder sprite, deliberately not copied from references.
    const auto body = state.sleeping ? kSleep : kAccent;
    r.fillRect(91, 35, 58, 45, body);
    r.fillRect(84, 43, 72, 27, body);
    r.fillRect(96, 29, 12, 12, body);
    r.fillRect(132, 29, 12, 12, body);
    r.fillRect(100, 50, 7, 7, kInk);
    r.fillRect(133, 50, 7, 7, kInk);
    r.fillRect(112, 64, 16, 4, kInk);
    r.fillRect(88, 79, 14, 5, body);
    r.fillRect(138, 79, 14, 5, body);
    if (state.sleeping) {
        r.drawText(160, 40, "Z", kSleep, 2);
        r.drawText(182, 28, "Z", kSleep);
    }

    const char* stage = "BABY";
    if (state.stage == core::EvolutionStage::Child) stage = "CHILD";
    if (state.stage == core::EvolutionStage::Adult) stage = "ADULT";
    char age[32]{};
    const auto ageMinutes = state.ageMillis / 60'000;
    std::snprintf(age, sizeof(age), "%s  AGE %llumin", stage,
                  static_cast<unsigned long long>(ageMinutes));
    r.drawText(63, 91, age, kInk);

    drawStat(r, 112, "HUNGER", state.hunger, state.hunger > 75 ? kAlert : kAccent,
             selectedStat == 0);
    drawStat(r, 134, "HAPPY", state.happiness, kAccent, selectedStat == 1);
    drawStat(r, 156, "ENERGY", state.energy, kSleep, selectedStat == 2);
    drawStat(r, 178, "HEALTH", state.health, state.health < 30 ? kAlert : kAccent,
             selectedStat == 3);

    r.fillRect(0, 207, 240, 33, kInk);
    r.drawText(8, 212, "F FEED  T TRAIN  S SLEEP", kPaper);
    r.drawText(8, 224, "TAB STAT  UP/DOWN  1-4 SPEED  A +1H", kPaper);
    r.endFrame();
}

}  // namespace neripal::ui
