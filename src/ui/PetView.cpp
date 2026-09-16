#include "neripal/ui/PetView.hpp"

#include "neripal/Version.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>

namespace neripal::ui {
namespace {
constexpr platform::Color kOutline = 0x001A2440;
constexpr platform::Color kInk = 0x002F3857;
constexpr platform::Color kSky = 0x0086C9D1;
constexpr platform::Color kSkyLight = 0x00D4F2D6;
constexpr platform::Color kDistantHill = 0x0059A7A2;
constexpr platform::Color kHill = 0x003F8073;
constexpr platform::Color kGrass = 0x007DBF68;
constexpr platform::Color kGrassLight = 0x00B6D975;
constexpr platform::Color kEarth = 0x0068A154;
constexpr platform::Color kPanel = 0x00E9D7B9;
constexpr platform::Color kPanelShade = 0x0084769D;
constexpr platform::Color kPanelLight = 0x00FFF0C9;
constexpr platform::Color kPurple = 0x0063549A;
constexpr platform::Color kGold = 0x00F6C75D;
constexpr platform::Color kPetMain = 0x005B79BC;
constexpr platform::Color kPetDark = 0x00394D83;
constexpr platform::Color kPetLight = 0x00A8D6D1;
constexpr platform::Color kAlert = 0x00D95850;
constexpr platform::Color kEnergy = 0x004F83C9;
constexpr platform::Color kHappy = 0x00DFAE52;
constexpr platform::Color kSleep = 0x006F75B3;

using Sprite = std::array<std::string_view, 16>;

constexpr Sprite kTadpoleSprite{
    "................",
    ".....OOOO.......",
    "...OOAAAAOO.....",
    "..OAAHAAHAAO....",
    "..OAAAAAAAAO....",
    "..OAAABBAAAO....",
    "...OAAAAAAO.....",
    "....OOOOOO......",
    ".......OO.......",
    "........OO......",
    ".........OO.....",
    "..........OO....",
    "...........OO...",
    "............OO..",
    ".............O..",
    "................",
};

constexpr Sprite kEggSprite{
    "................",
    "......OOOO......",
    ".....OEEEEO.....",
    "....OEEHEEEO....",
    "...OEEEEEEEEO...",
    "...OEESEEESEEO..",
    "...OEEEEEEEEO...",
    "...OEESEEEEEO...",
    "...OEEEEEEEEO...",
    "....OEEEEEEO....",
    ".....OEEEEO.....",
    "......OOOO......",
    "................",
    "................",
    "................",
    "................",
};

constexpr Sprite kLeggedTadpoleSprite{
    ".....OOOO.......",
    "...OOAAAAOO.....",
    "..OAAHAAHAAO....",
    "..OAAAAAAAAO....",
    "..OAAABBAAAO....",
    "...OAAAAAAO.....",
    "....OOAAOO......",
    "...O..AA..O.....",
    "..O...AA...O....",
    "..O...AA...O....",
    "...O..AA..O.....",
    "........OO......",
    ".........OO.....",
    "..........OO....",
    "...........OO...",
    "................",
};

constexpr Sprite kFrogSprite{
    "....OO....OO....",
    "...OAAO..OAAO...",
    "...OAAAAAAAAO...",
    "...OAAHAAHAAO...",
    "....OAAAAAAO....",
    "....OABBBBAO....",
    "...OAAAAAAAAO...",
    "..OAAO.OO.OAAO..",
    ".OAAO..OO..OAAO.",
    "OAAO........OAAO",
    "OAO..........OAO",
    ".OO..........OO.",
    "................",
    "................",
    "................",
    "................",
};

const Sprite& spriteForStage(core::EvolutionStage stage) {
    switch (stage) {
        case core::EvolutionStage::Egg: return kEggSprite;
        case core::EvolutionStage::Child: return kLeggedTadpoleSprite;
        case core::EvolutionStage::Adult: return kFrogSprite;
        default: return kTadpoleSprite;
    }
}

void drawPanel(platform::IRenderer& r, int x, int y, int width, int height,
               platform::Color fill, platform::Color border = kOutline) {
    r.fillRect(x + 3, y + 3, width - 3, height - 3, kPanelShade);
    r.fillRect(x + 3, y, width - 6, height, border);
    r.fillRect(x, y + 3, width, height - 6, border);
    r.fillRect(x + 4, y + 4, width - 8, height - 8, fill);
    r.fillRect(x + 6, y + 5, width - 12, 2, kPanelLight);
}

void drawCloud(platform::IRenderer& r, int x, int y) {
    r.fillRect(x, y + 5, 30, 7, kSkyLight);
    r.fillRect(x + 6, y + 1, 13, 12, kSkyLight);
    r.fillRect(x + 17, y + 3, 11, 10, kSkyLight);
}

void drawTree(platform::IRenderer& r, int x, int y) {
    r.fillRect(x + 8, y + 16, 5, 17, kEarth);
    r.fillRect(x + 1, y + 7, 19, 14, kHill);
    r.fillRect(x + 5, y + 2, 12, 16, kDistantHill);
    r.fillRect(x + 8, y, 7, 8, kGrassLight);
}

void drawScene(platform::IRenderer& r) {
    r.fillRect(0, 24, 240, 183, kSky);
    r.fillRect(0, 70, 240, 58, kSkyLight);
    drawCloud(r, 20, 40);
    drawCloud(r, 171, 55);
    r.fillRect(0, 104, 240, 42, kDistantHill);
    r.fillRect(0, 119, 240, 35, kHill);
    r.fillRect(0, 145, 240, 62, kGrass);
    r.fillRect(0, 176, 240, 31, kEarth);
    r.fillRect(0, 151, 240, 8, kGrassLight);
    drawTree(r, 18, 105);
    drawTree(r, 199, 101);
    r.fillRect(58, 182, 22, 4, kGrassLight);
    r.fillRect(154, 193, 28, 4, kGrassLight);
    r.fillRect(100, 170, 7, 4, kPanelLight);
    r.fillRect(109, 174, 5, 3, kPanelLight);
}

void drawHud(platform::IRenderer& r, const char* label) {
    r.fillRect(0, 0, 240, 25, kOutline);
    r.fillRect(4, 4, 26, 17, kPurple);
    r.drawRect(4, 4, 26, 17, kPanelLight);
    r.drawText(11, 8, "N", kPanelLight);
    r.drawText(39, 8, version::kScreenLabel, kPanelLight);
    r.fillRect(187, 4, 49, 17, kPurple);
    r.drawRect(187, 4, 49, 17, kPanelLight);
    r.drawText(194, 8, label, kPanelLight);
}

void drawDockIcon(platform::IRenderer& r, int x, bool selected, const char* glyph) {
    drawPanel(r, x, 211, 28, 22, selected ? kGold : kPanel, selected ? kGold : kOutline);
    r.drawText(x + 9, 218, glyph, kInk, 1);
}

void drawStatsIcon(platform::IRenderer& r, int x, int y) {
    r.fillRect(x, y, 28, 22, kPurple);
    r.fillRect(x + 5, y + 5, 5, 12, kPanelLight);
    r.fillRect(x + 13, y + 9, 5, 8, kGold);
    r.fillRect(x + 21, y + 3, 3, 14, kPetLight);
}

void drawHomeIcon(platform::IRenderer& r, int x, int y) {
    r.fillRect(x + 5, y + 10, 18, 11, kPurple);
    r.fillRect(x + 8, y + 5, 12, 7, kPurple);
    r.fillRect(x + 11, y + 14, 5, 7, kPanelLight);
}

const char* stageLabel(core::EvolutionStage stage) {
    switch (stage) {
        case core::EvolutionStage::Egg: return "EGG";
        case core::EvolutionStage::Child: return "CHILD";
        case core::EvolutionStage::Adult: return "ADULT";
        default: return "BABY";
    }
}

const char* moodLabel(const core::PetState& state) {
    if (state.hunger > 75 || state.health < 30) return "WORRIED";
    if (state.sleeping) return "RESTING";
    if (state.happiness > 80) return "HAPPY";
    return "CALM";
}
}

void PetView::drawStat(platform::IRenderer& r, int y, const char* label,
                       int value, platform::Color color) {
    r.drawText(25, y, label, kInk);
    r.fillRect(54, y + 1, 59, 8, kPanelShade);
    r.fillRect(56, y + 3, 55, 4, kPanelLight);
    const int fill = std::clamp(value, 0, 100) * 55 / 100;
    r.fillRect(56, y + 3, fill, 4, color);
    char buffer[5]{};
    std::snprintf(buffer, sizeof(buffer), "%3d", value);
    r.drawText(115, y, buffer, kInk);
}

void PetView::drawPet(platform::IRenderer& r, int x, int y, int idleFrame, bool sleeping,
                      core::EvolutionStage stage) {
    constexpr int kPixel = 4;
    const auto& sprite = spriteForStage(stage);
    const int bob = sleeping ? 0 : idleFrame * 2;
    for (std::size_t row = 0; row < sprite.size(); ++row) {
        for (std::size_t column = 0; column < sprite[row].size(); ++column) {
            platform::Color color = 0;
            switch (sprite[row][column]) {
                case 'O': color = kOutline; break;
                case 'A': color = sleeping ? kSleep : kPetMain; break;
                case 'B': color = sleeping ? kPurple : kPetDark; break;
                case 'H': color = sleeping ? kPanelLight : kPetLight; break;
                case 'E': color = kPanelLight; break;
                case 'S': color = kGold; break;
                default: continue;
            }
            r.fillRect(x + static_cast<int>(column) * kPixel,
                       y + static_cast<int>(row) * kPixel + bob,
                       kPixel, kPixel, color);
        }
    }
    if (sleeping) {
        r.drawText(x + 66, y + 12, "Z", kPanelLight, 2);
        r.drawText(x + 83, y + 4, "Z", kPanelLight);
    }
}

void PetView::render(platform::IRenderer& r, const core::PetState& state,
                     const UiState& uiState) const {
    r.beginFrame(kSky);
    drawScene(r);

    if (uiState.screen == Screen::Home) {
        drawHud(r, "HOME");
        drawPet(r, 88, 67, uiState.idleFrame, state.sleeping, state.stage);
        drawPanel(r, 57, 143, 126, 29, kPanel);
        r.drawText(73, 153, state.stage == core::EvolutionStage::Egg ? "WAITING"
                                                                        : moodLabel(state),
                   kInk, 2);
        r.fillRect(0, 207, 240, 33, kOutline);
        drawDockIcon(r, 36, false, "*");
        drawDockIcon(r, 75, false, "+");
        drawDockIcon(r, 114, false, "i");
        drawDockIcon(r, 153, true, ">");
        r.drawText(188, 218, "B", kPanelLight, 2);
        r.drawText(184, 227, "MENU", kPanelLight);
    } else if (uiState.screen == Screen::MainMenu) {
        drawHud(r, "MENU");
        drawPanel(r, 16, 48, 208, 132, kPanel);
        r.drawText(95, 58, "MENU", kInk, 2);

        constexpr int kFirstTileX = 47;
        constexpr int kTileSpacing = 92;
        const float from = static_cast<float>(uiState.previousMenuIndex);
        const float to = static_cast<float>(uiState.menuIndex);
        const int highlightX = kFirstTileX + static_cast<int>(
            (from + (to - from) * uiState.menuSelectionProgress) * kTileSpacing);
        drawPanel(r, highlightX - 5, 79, 76, 69, kGold, kGold);
        drawStatsIcon(r, kFirstTileX + 18, 94);
        drawHomeIcon(r, kFirstTileX + kTileSpacing + 18, 94);
        r.drawText(kFirstTileX + 7, 157, "STATUS", kInk);
        r.drawText(kFirstTileX + kTileSpacing + 10, 157, "HOME", kInk);
        r.fillRect(0, 207, 240, 33, kOutline);
        r.drawText(28, 216, "A NEXT", kPanelLight);
        r.drawText(105, 216, "B OK", kPanelLight);
        r.drawText(165, 216, "C BACK", kPanelLight);
        r.drawText(78, 228, "SELECT AN ICON", kGold);
    } else {
        drawHud(r, "STATUS");
        drawPanel(r, 10, 39, 220, 157, kPanel);
        r.drawText(23, 49, "VITAL SIGNS", kInk, 2);
        drawPanel(r, 19, 71, 119, 109, kPanelLight, kPurple);
        drawStat(r, 82, "HUN", state.hunger, state.hunger > 75 ? kAlert : kHappy);
        drawStat(r, 104, "HAP", state.happiness, kHappy);
        drawStat(r, 126, "ENG", state.energy, kEnergy);
        drawStat(r, 148, "HP ", state.health, state.health < 30 ? kAlert : kGrass);
        r.drawText(150, 76, stageLabel(state.stage), kInk);
        drawPet(r, 151, 88, uiState.idleFrame, state.sleeping, state.stage);
        r.drawText(153, 163, state.stage == core::EvolutionStage::Egg ? "WAITING"
                                                                        : moodLabel(state),
                   kInk);
        char age[18]{};
        std::snprintf(age, sizeof(age), "AGE %llum",
                      static_cast<unsigned long long>(state.ageMillis / 60'000));
        r.drawText(153, 176, age, kInk);
        r.fillRect(0, 207, 240, 33, kOutline);
        r.drawText(81, 219, "C  BACK", kPanelLight, 2);
    }
    r.endFrame();
}

}  // namespace neripal::ui
