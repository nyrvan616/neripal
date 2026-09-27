#include "neripal/ui/PetView.hpp"

#include "neripal/Version.hpp"
#include "neripal/core/Activity.hpp"
#include "neripal/core/Balance.hpp"
#include "neripal/core/Mood.hpp"
#include "neripal/ui/NeedSignals.hpp"

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
        case core::EvolutionStage::Baby: return kTadpoleSprite;
        case core::EvolutionStage::Child: return kLeggedTadpoleSprite;
        case core::EvolutionStage::Adult: return kFrogSprite;
        case core::EvolutionStage::Final: return kFrogSprite;
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

void drawPetIcon(platform::IRenderer& r, int x, int y) {
    r.fillRect(x + 6, y + 6, 7, 7, kAlert);
    r.fillRect(x + 15, y + 6, 7, 7, kAlert);
    r.fillRect(x + 8, y + 12, 12, 6, kAlert);
    r.fillRect(x + 11, y + 17, 6, 3, kAlert);
}

void drawPlayIcon(platform::IRenderer& r, int x, int y) {
    r.fillRect(x + 8, y + 6, 12, 12, kEnergy);
    r.drawRect(x + 8, y + 6, 12, 12, kOutline);
    r.fillRect(x + 12, y + 9, 5, 6, kPanelLight);
}

std::pair<int, int> menuTileOrigin(int menuIndex) {
    constexpr int kColumns = 4;
    constexpr int kOriginX = 20;
    constexpr int kOriginY = 76;
    constexpr int kColSpacing = 52;
    constexpr int kRowSpacing = 48;
    return {kOriginX + (menuIndex % kColumns) * kColSpacing,
            kOriginY + (menuIndex / kColumns) * kRowSpacing};
}

void drawMenuIcon(platform::IRenderer& r, int menuIndex, bool sleeping) {
    const auto [x, y] = menuTileOrigin(menuIndex);
    switch (menuIndex) {
        case UiController::kMenuFeed: drawFeedIcon(r, x + 2, y + 2); break;
        case UiController::kMenuTrain: drawTrainIcon(r, x + 2, y + 2); break;
        case UiController::kMenuSleep: drawSleepIcon(r, x + 2, y + 2, sleeping); break;
        case UiController::kMenuClean: drawCleanIcon(r, x + 2, y + 2); break;
        case UiController::kMenuPet: drawPetIcon(r, x + 2, y + 2); break;
        case UiController::kMenuPlay: drawPlayIcon(r, x + 2, y + 2); break;
        case UiController::kMenuStatus: drawStatsIcon(r, x + 2, y + 2); break;
        case UiController::kMenuHome: drawHomeIcon(r, x + 2, y + 2); break;
        default: break;
    }
}

const char* menuLabel(int menuIndex, bool sleeping) {
    switch (menuIndex) {
        case UiController::kMenuFeed: return "FEED";
        case UiController::kMenuTrain: return "TRAIN";
        case UiController::kMenuSleep: return sleeping ? "WAKE" : "SLEEP";
        case UiController::kMenuClean: return "CLEAN";
        case UiController::kMenuPet: return "PET";
        case UiController::kMenuPlay: return "PLAY";
        case UiController::kMenuStatus: return "STATUS";
        case UiController::kMenuHome: return "HOME";
        default: return "";
    }
}

const char* formLabel(core::FormId form) {
    switch (form) {
        case core::FormId::None: return "NONE";
        case core::FormId::Juvenile: return "JUVENILE";
        case core::FormId::AdultA: return "ADULT-A";
        case core::FormId::AdultB: return "ADULT-B";
        case core::FormId::AdultC: return "ADULT-C";
        case core::FormId::AdultSecret: return "SECRET";
    }
    return "NONE";
}

const char* stageLabel(core::EvolutionStage stage) {
    switch (stage) {
        case core::EvolutionStage::Egg: return "EGG";
        case core::EvolutionStage::Baby: return "BABY";
        case core::EvolutionStage::Child: return "CHILD";
        case core::EvolutionStage::Adult: return "ADULT";
        case core::EvolutionStage::Final: return "FINAL";
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
        case core::CareResult::RejectedEgg: return "EGG";
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
        case core::CareAction::Pet: return "PAT";
        case core::CareAction::Play: return "PLAY";
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
        case core::CareAction::Pet:
            r.fillRect(150, 96, 6, 6, kAlert);
            r.fillRect(158, 96, 6, 6, kAlert);
            r.fillRect(150, 100, 14, 6, kAlert);
            break;
        case core::CareAction::Play:
            r.fillRect(148, 100, 12, 12, kEnergy);
            r.drawRect(148, 100, 12, 12, kOutline);
            break;
    }
    drawPanel(r, 154, 36, 62, 24, kPanel);
    r.drawText(162, 44, appliedLabel(uiState.careAction), kInk);
}

void drawNeedGlyph(platform::IRenderer& r, int x, int y, core::Need need) {
    switch (need) {
        case core::Need::Hunger:
            r.fillRect(x + 2, y + 8, 14, 4, kInk);
            r.fillRect(x + 4, y + 4, 3, 6, kInk);
            r.fillRect(x + 11, y + 4, 3, 6, kInk);
            break;
        case core::Need::Energy:
            r.fillRect(x + 8, y + 1, 4, 5, kInk);
            r.fillRect(x + 5, y + 6, 4, 5, kInk);
            r.fillRect(x + 8, y + 11, 4, 5, kInk);
            break;
        case core::Need::Hygiene:
            r.fillRect(x + 7, y + 2, 4, 4, kInk);
            r.fillRect(x + 5, y + 6, 8, 4, kInk);
            r.fillRect(x + 6, y + 10, 6, 4, kInk);
            break;
        case core::Need::Affection:
            r.fillRect(x + 2, y + 4, 6, 6, kInk);
            r.fillRect(x + 10, y + 4, 6, 6, kInk);
            r.fillRect(x + 5, y + 9, 8, 5, kInk);
            break;
        case core::Need::Stimulation:
            r.fillRect(x + 7, y + 2, 4, 12, kInk);
            r.fillRect(x + 3, y + 6, 12, 4, kInk);
            break;
    }
}

void drawNeedSignals(platform::IRenderer& r, const core::PetState& state) {
    const NeedSignalList signals = needSignals(state);
    constexpr int kStep = 44;
    int x = 12;
    for (std::uint8_t i = 0; i < signals.count; ++i) {
        const NeedSignal& signal = signals.items[i];
        const bool urgent = signal.level == core::NeedLevel::Urgent;
        drawNeedGlyph(r, x, 30, signal.need);
        r.drawText(x + 18, 32, needSymbol(signal.need), kInk);
        if (urgent) {
            drawNeedGlyph(r, x + 1, 31, signal.need);
            r.drawRect(x - 2, 28, 40, 18, kOutline);
            r.drawRect(x - 4, 26, 44, 22, kOutline);
            r.drawText(x + 18, 42, "!", kInk);
        } else {
            r.drawRect(x - 2, 28, 40, 18, kOutline);
        }
        x += kStep;
    }
}

void drawEvolutionNotice(platform::IRenderer& r, const UiState& uiState) {
    if (!uiState.evolutionNoticeVisible) return;
    drawPanel(r, 24, 64, 192, 112, kPanel);
    r.drawText(40, 76, "EVOLVED", kInk, 2);
    char transition[24]{};
    std::snprintf(transition, sizeof(transition), "%s > %s", stageLabel(uiState.evolutionFrom),
                  stageLabel(uiState.evolutionTo));
    r.drawText(40, 100, transition, kInk);
    r.drawText(40, 116, "FORM", kInk);
    r.drawText(100, 116, formLabel(uiState.evolutionForm), kInk);
    r.drawText(40, 148, "B CONFIRM", kInk);
}

platform::Color needBarColor(core::Need need, int value, platform::Color normal) {
    switch (core::needLevel(need, value)) {
        case core::NeedLevel::Urgent: return kAlert;
        case core::NeedLevel::Attention: return kGold;
        case core::NeedLevel::Normal: return normal;
    }
    return normal;
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
        drawNeedSignals(r, state);
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
        drawPanel(r, highlightX - 2, highlightY - 2, 48, 42, kGold, kGold);
        for (int menuIndex = 0; menuIndex < UiController::kMenuItemCount; ++menuIndex) {
            drawMenuIcon(r, menuIndex, state.sleeping);
            const auto [labelX, labelY] = menuTileOrigin(menuIndex);
            r.drawText(labelX, labelY + 26, menuLabel(menuIndex, state.sleeping), kInk);
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
        drawStat(r, 78, "HUN", state.hunger, needBarColor(core::Need::Hunger, state.hunger, kHappy));
        drawStat(r, 90, "NRG", state.energy, needBarColor(core::Need::Energy, state.energy, kEnergy));
        drawStat(r, 102, "HYG", state.hygiene,
                 needBarColor(core::Need::Hygiene, state.hygiene, kPetLight));
        drawStat(r, 114, "AFE", state.affection,
                 needBarColor(core::Need::Affection, state.affection, kPetMain));
        drawStat(r, 126, "STM", state.stimulation,
                 needBarColor(core::Need::Stimulation, state.stimulation, kEnergy));
        drawStat(r, 138, "HAP", state.happiness, kHappy);
        drawStat(r, 150, "HP", state.health,
                 state.health < core::balance::kAnnoyedMoodHealth ? kAlert : kGrass);
        r.drawText(148, 80, "STAGE", kInk);
        r.drawText(148, 94, stageLabel(state.stage), kInk);
        r.drawText(148, 116, "FORM", kInk);
        r.drawText(148, 130, formLabel(state.form), kInk);
        r.drawText(148, 156, careMoodBanner(state), kInk);
        char age[18]{};
        std::snprintf(age, sizeof(age), "AGE %llum",
                      static_cast<unsigned long long>(state.ageMillis / 60'000));
        r.drawText(148, 172, age, kInk);
        r.fillRect(0, 207, 240, 33, kOutline);
        r.drawText(81, 219, "C  BACK", kPanelLight, 2);
    }
    drawEvolutionNotice(r, uiState);
    r.endFrame();
}

}  // namespace neripal::ui
