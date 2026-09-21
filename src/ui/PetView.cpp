#include "neripal/ui/PetView.hpp"

#include "neripal/Version.hpp"
#include "neripal/core/Activity.hpp"
#include "neripal/core/Balance.hpp"
#include "neripal/core/Mood.hpp"

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

void drawFeedIcon(platform::IRenderer& r, int x, int y) {
    r.fillRect(x + 4, y + 14, 20, 6, kPurple);
    r.fillRect(x + 7, y + 10, 14, 5, kGold);
    r.fillRect(x + 10, y + 6, 8, 5, kHappy);
}

void drawTrainIcon(platform::IRenderer& r, int x, int y) {
    r.fillRect(x + 2, y + 10, 6, 6, kPetDark);
    r.fillRect(x + 20, y + 10, 6, 6, kPetDark);
    r.fillRect(x + 7, y + 12, 14, 3, kEnergy);
}

void drawSleepIcon(platform::IRenderer& r, int x, int y, bool sleeping) {
    r.fillRect(x + 6, y + 8, 16, 12, sleeping ? kSleep : kPurple);
    r.drawText(x + (sleeping ? 8 : 10), y + 10, sleeping ? "Z" : "ZZ", kPanelLight, 1);
}

void drawCleanIcon(platform::IRenderer& r, int x, int y) {
    r.fillRect(x + 12, y + 4, 4, 12, kPetLight);
    r.fillRect(x + 6, y + 10, 16, 4, kPetLight);
    r.fillRect(x + 4, y + 6, 4, 4, kGold);
    r.fillRect(x + 20, y + 14, 4, 4, kGold);
}

std::pair<int, int> menuTileOrigin(int menuIndex) {
    constexpr int kOriginX = 28;
    constexpr int kOriginY = 76;
    constexpr int kColSpacing = 68;
    constexpr int kRowSpacing = 52;
    return {kOriginX + (menuIndex % 3) * kColSpacing, kOriginY + (menuIndex / 3) * kRowSpacing};
}

void drawMenuIcon(platform::IRenderer& r, int menuIndex, bool sleeping) {
    const auto [x, y] = menuTileOrigin(menuIndex);
    switch (menuIndex) {
        case 0: drawFeedIcon(r, x + 3, y + 2); break;
        case 1: drawTrainIcon(r, x + 3, y + 2); break;
        case 2: drawSleepIcon(r, x + 3, y + 2, sleeping); break;
        case 3: drawCleanIcon(r, x + 3, y + 2); break;
        case 4: drawStatsIcon(r, x + 3, y + 2); break;
        case 5: drawHomeIcon(r, x + 3, y + 2); break;
        default: break;
    }
}

const char* menuLabel(int menuIndex, bool sleeping) {
    switch (menuIndex) {
        case 0: return "FEED";
        case 1: return "TRAIN";
        case 2: return sleeping ? "WAKE" : "SLEEP";
        case 3: return "CLEAN";
        case 4: return "STATUS";
        case 5: return "HOME";
        default: return "";
    }
}

const char* stageLabel(core::EvolutionStage stage) {
    switch (stage) {
        case core::EvolutionStage::Egg: return "EGG";
        case core::EvolutionStage::Child: return "CHILD";
        case core::EvolutionStage::Adult: return "ADULT";
        default: return "BABY";
    }
}

const char* moodLabel(core::Mood mood) {
    switch (mood) {
        case core::Mood::Tired: return "TIRED";
        case core::Mood::Dirty: return "DIRTY";
        case core::Mood::Annoyed: return "ANNOYED";
        case core::Mood::Resting: return "RESTING";
        case core::Mood::Happy: return "HAPPY";
        case core::Mood::Calm: return "CALM";
    }
    return "CALM";
}

const char* careMoodBanner(const core::PetState& state) {
    if (state.stage == core::EvolutionStage::Egg) return "WAITING";
    return moodLabel(core::deriveMood(state));
}

const char* rejectionLabel(core::CareResult result) {
    switch (result) {
        case core::CareResult::RejectedAsleep: return "ASLEEP";
        case core::CareResult::RejectedNoEnergy: return "TIRED";
        case core::CareResult::RejectedAlreadySleeping: return "RESTING";
        case core::CareResult::RejectedAlreadyAwake: return "AWAKE";
        default: return nullptr;
    }
}

const char* appliedLabel(core::CareAction action) {
    switch (action) {
        case core::CareAction::Feed: return "EAT";
        case core::CareAction::Train: return "GO";
        case core::CareAction::Sleep: return "ZZZ";
        case core::CareAction::Wake: return "UP";
        case core::CareAction::Clean: return "WASH";
    }
    return "";
}

void drawCareOverlay(platform::IRenderer& r, const UiState& uiState) {
    if (!uiState.careFeedbackActive) return;
    if (uiState.careResult != core::CareResult::Applied) return;

    switch (uiState.careAction) {
        case core::CareAction::Feed:
            r.fillRect(148, 102, 14, 10, kGold);
            r.fillRect(151, 105, 8, 4, kHappy);
            break;
        case core::CareAction::Train:
            r.fillRect(80, 78, 6, 6, kEnergy);
            r.fillRect(154, 90, 6, 6, kEnergy);
            break;
        case core::CareAction::Sleep:
            r.drawText(158, 48, "Z", kPanelLight, 2);
            break;
        case core::CareAction::Wake:
            r.fillRect(156, 50, 18, 8, kGold);
            break;
        case core::CareAction::Clean:
            r.fillRect(80, 62, 4, 4, kGold);
            r.fillRect(156, 70, 4, 4, kPanelLight);
            r.fillRect(84, 126, 4, 4, kGold);
            r.fillRect(150, 118, 4, 4, kPanelLight);
            break;
    }
    drawPanel(r, 154, 36, 62, 24, kPanel);
    r.drawText(162, 44, appliedLabel(uiState.careAction), kInk);
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

void PetView::drawPet(platform::IRenderer& r, int x, int y, int idleFrame, int extraBob,
                      const core::PetState& state) {
    constexpr int kPixel = 4;
    constexpr int kSpriteCells = 16;
    const auto& sprite = spriteForStage(state.stage);
    const bool asleep = state.sleeping || state.activity == core::Activity::Sleep ||
                        state.activity == core::Activity::Nap;
    const core::Mood mood = core::deriveMood(state);

    int frame = idleFrame;
    int amplitude = 2;
    if (state.activity == core::Activity::Walk) {
        frame = static_cast<int>((state.activityElapsedMs / 250u) % 2u);
        amplitude = 2;
    } else if (mood == core::Mood::Tired || state.idleVariant == core::IdleVariant::Slump) {
        amplitude = 1;
    }
    if (state.activity == core::Activity::Happy ||
        state.idleVariant == core::IdleVariant::Bounce) {
        extraBob += 2;
    }
    const int bob = asleep ? 0 : frame * amplitude + extraBob;
    const bool flip = state.facing < 0;

    for (std::size_t row = 0; row < sprite.size(); ++row) {
        for (std::size_t column = 0; column < sprite[row].size(); ++column) {
            platform::Color color = 0;
            switch (sprite[row][column]) {
                case 'O': color = kOutline; break;
                case 'A': color = asleep ? kSleep : kPetMain; break;
                case 'B': color = asleep ? kPurple : kPetDark; break;
                case 'H': color = asleep ? kPanelLight : kPetLight; break;
                case 'E': color = kPanelLight; break;
                case 'S': color = kGold; break;
                default: continue;
            }
            const int cell = flip ? (kSpriteCells - 1 - static_cast<int>(column))
                                  : static_cast<int>(column);
            r.fillRect(x + cell * kPixel, y + static_cast<int>(row) * kPixel + bob, kPixel,
                       kPixel, color);
        }
    }

    if (asleep) {
        const int z1x = std::min(x + 66, platform::IRenderer::kLogicalWidth - 12);
        const int z2x = std::min(x + 83, platform::IRenderer::kLogicalWidth - 8);
        r.drawText(z1x, y + 12, "Z", kPanelLight, 2);
        r.drawText(z2x, y + 4, "Z", kPanelLight);
    }

    const bool showDirty = mood == core::Mood::Dirty || state.activity == core::Activity::Dirty ||
                           state.idleVariant == core::IdleVariant::Shake;
    if (showDirty && !asleep) {
        r.fillRect(std::clamp(x + 8, 0, 236), std::clamp(y + 18 + bob, 0, 236), 4, 4, kAlert);
        r.fillRect(std::clamp(x + 48, 0, 236), std::clamp(y + 40 + bob, 0, 236), 4, 4, kAlert);
        r.fillRect(std::clamp(x + 28, 0, 236), std::clamp(y + 8 + bob, 0, 236), 3, 3, kEarth);
    }

    const bool showAnnoyed = mood == core::Mood::Annoyed ||
                             state.activity == core::Activity::Annoyed ||
                             state.idleVariant == core::IdleVariant::Fidget;
    if (showAnnoyed && !asleep) {
        r.fillRect(std::clamp(x + 16, 0, 236), std::clamp(y + 10 + bob, 0, 236), 8, 2, kAlert);
        r.fillRect(std::clamp(x + 36, 0, 236), std::clamp(y + 10 + bob, 0, 236), 8, 2, kAlert);
    }

    if (state.activity == core::Activity::Eat) {
        const int foodX = std::clamp(x + (flip ? -18 : 52), 0, 226);
        const int foodY = std::clamp(y + 38 + bob, 0, 230);
        r.fillRect(foodX, foodY, 14, 10, kGold);
        r.fillRect(foodX + 3, foodY + 3, 8, 4, kHappy);
    }
}

void PetView::render(platform::IRenderer& r, const core::PetState& state,
                     const UiState& uiState) const {
    r.beginFrame(kSky);
    drawScene(r);

    if (uiState.screen == Screen::Home) {
        drawHud(r, "HOME");
        const int extraBob =
            (uiState.careFeedbackActive && uiState.careResult == core::CareResult::Applied &&
             uiState.careAction == core::CareAction::Train)
                ? 4
                : 0;
        const int petX = std::clamp(state.x, 0, platform::IRenderer::kLogicalWidth - 64);
        drawPet(r, petX, 67, uiState.idleFrame, extraBob, state);
        drawCareOverlay(r, uiState);
        drawPanel(r, 57, 143, 126, 29, kPanel);
        const char* banner = nullptr;
        if (uiState.careFeedbackActive) {
            banner = rejectionLabel(uiState.careResult);
        }
        if (banner == nullptr) {
            banner = careMoodBanner(state);
        }
        r.drawText(73, 153, banner, kInk, 2);
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
        r.drawText(95, 58, "CARE", kInk, 2);

        const auto [fromX, fromY] = menuTileOrigin(uiState.previousMenuIndex);
        const auto [toX, toY] = menuTileOrigin(uiState.menuIndex);
        const float t = uiState.menuSelectionProgress;
        const int highlightX = fromX + static_cast<int>((toX - fromX) * t);
        const int highlightY = fromY + static_cast<int>((toY - fromY) * t);
        drawPanel(r, highlightX - 4, highlightY - 4, 58, 46, kGold, kGold);
        for (int menuIndex = 0; menuIndex < UiController::kMenuItemCount; ++menuIndex) {
            drawMenuIcon(r, menuIndex, state.sleeping);
            const auto [labelX, labelY] = menuTileOrigin(menuIndex);
            r.drawText(labelX + 2, labelY + 34, menuLabel(menuIndex, state.sleeping), kInk);
        }
        r.fillRect(0, 207, 240, 33, kOutline);
        r.drawText(28, 216, "A NEXT", kPanelLight);
        r.drawText(105, 216, "B OK", kPanelLight);
        r.drawText(165, 216, "C BACK", kPanelLight);
        r.drawText(78, 228, "SELECT AN ICON", kGold);
    } else {
        drawHud(r, "STATUS");
        drawPanel(r, 10, 39, 220, 157, kPanel);
        r.drawText(23, 49, "VITAL SIGNS", kInk, 2);
        drawPanel(r, 19, 71, 119, 125, kPanelLight, kPurple);
        drawStat(r, 76, "HUN", state.hunger,
                 state.hunger >= core::balance::kAnnoyedMoodHunger ? kAlert : kHappy);
        drawStat(r, 94, "HAP", state.happiness, kHappy);
        drawStat(r, 112, "ENG", state.energy, kEnergy);
        drawStat(r, 130, "HP ", state.health,
                 state.health < core::balance::kAnnoyedMoodHealth ? kAlert : kGrass);
        drawStat(r, 148, "HYG", state.hygiene,
                 state.hygiene <= core::balance::kHygieneNeglectThreshold ? kAlert : kPetLight);
        r.drawText(150, 76, stageLabel(state.stage), kInk);
        drawPet(r, 151, 88, uiState.idleFrame, 0, state);
        r.drawText(153, 163, careMoodBanner(state), kInk);
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
