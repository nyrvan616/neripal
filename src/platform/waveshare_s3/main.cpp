#include "Esp32WallClock.hpp"
#include "NvsSaveStorage.hpp"
#include "WaveshareS3Platform.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/core/XorShift32.hpp"
#include "neripal/persist/SaveSession.hpp"
#include "neripal/core/EvolutionNotice.hpp"
#include "neripal/ui/NeedSignals.hpp"
#include "neripal/ui/PetView.hpp"
#include "neripal/ui/UiController.hpp"

#include <Arduino.h>
#include <cstdint>

namespace {
// 0.4 placeholder until a platform entropy source exists (ADC, esp_random, or NVS).
// Chosen by this composition root; not a firmware product contract and not seed 1.
constexpr std::uint32_t kFirmwareRngSeedPlaceholder = 0x57415645u;  // 'WAVE'

neripal::waveshare_s3::Esp32Clock gameClock;
neripal::waveshare_s3::Esp32Clock sessionClock;
neripal::waveshare_s3::Esp32WallClock wallClock;
neripal::waveshare_s3::NvsSaveStorage storage;
neripal::core::XorShift32 rng(kFirmwareRngSeedPlaceholder);
neripal::core::Pet pet(gameClock, rng);
neripal::persist::SaveSession saves(pet, storage, wallClock, sessionClock);
neripal::waveshare_s3::WaveshareS3Platform platform;
neripal::ui::PetView view;
neripal::ui::UiController ui;
// Read by a future buzzer. Home glyphs are the mandatory urgent signal.
volatile bool urgentSoundCue = false;

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

#if NERIPAL_RENDER_DIAGNOSTICS
constexpr std::uint32_t kRenderDiagPeriodMs = 2000;

struct RenderDiag {
    std::uint32_t windowStartMs = 0;
    std::uint32_t frames = 0;
    std::uint64_t frameUs = 0;
    std::uint64_t flushUs = 0;
    bool open = false;
};

RenderDiag renderDiag;

const char* screenName(neripal::ui::Screen screen) {
    switch (screen) {
        case neripal::ui::Screen::Home: return "HOME";
        case neripal::ui::Screen::MainMenu: return "MENU";
        case neripal::ui::Screen::Status: return "STATUS";
    }
    return "?";
}

const char* menuName(int index, bool sleeping) {
    switch (index) {
        case neripal::ui::UiController::kMenuFeed: return "FEED";
        case neripal::ui::UiController::kMenuTrain: return "TRAIN";
        case neripal::ui::UiController::kMenuSleep: return sleeping ? "WAKE" : "SLEEP";
        case neripal::ui::UiController::kMenuClean: return "CLEAN";
        case neripal::ui::UiController::kMenuPet: return "PET";
        case neripal::ui::UiController::kMenuPlay: return "PLAY";
        case neripal::ui::UiController::kMenuStatus: return "STATUS";
        case neripal::ui::UiController::kMenuHome: return "HOME";
        default: return "?";
    }
}

const char* stageName(neripal::core::EvolutionStage stage) {
    switch (stage) {
        case neripal::core::EvolutionStage::Egg: return "EGG";
        case neripal::core::EvolutionStage::Baby: return "BABY";
        case neripal::core::EvolutionStage::Child: return "CHILD";
        case neripal::core::EvolutionStage::Adult: return "ADULT";
        case neripal::core::EvolutionStage::Final: return "FINAL";
    }
    return "?";
}

const char* formName(neripal::core::FormId form) {
    switch (form) {
        case neripal::core::FormId::None: return "NONE";
        case neripal::core::FormId::Juvenile: return "JUVENILE";
        case neripal::core::FormId::AdultA: return "ADULT-A";
        case neripal::core::FormId::AdultB: return "ADULT-B";
        case neripal::core::FormId::AdultC: return "ADULT-C";
        case neripal::core::FormId::AdultSecret: return "SECRET";
    }
    return "?";
}

void notePresentedNotice(const neripal::core::EvolutionNotice& notice) {
    Serial.printf("[LCD] notice from=%s to=%s form=%s\n", stageName(notice.from),
                  stageName(notice.to), formName(notice.form));
}

void noteRenderSample(std::uint32_t frameUs, std::uint32_t flushUs) {
    const std::uint32_t nowMs = millis();
    if (!renderDiag.open) {
        renderDiag.windowStartMs = nowMs;
        renderDiag.open = true;
    }
    ++renderDiag.frames;
    renderDiag.frameUs += frameUs;
    renderDiag.flushUs += flushUs;
    const std::uint32_t elapsedMs = nowMs - renderDiag.windowStartMs;
    if (elapsedMs < kRenderDiagPeriodMs || renderDiag.frames == 0) return;

    const std::uint32_t avgFrameUs =
        static_cast<std::uint32_t>(renderDiag.frameUs / renderDiag.frames);
    const std::uint32_t avgFlushUs =
        static_cast<std::uint32_t>(renderDiag.flushUs / renderDiag.frames);
    const std::uint32_t fps = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(renderDiag.frames) * 1000u) / elapsedMs);
    const neripal::ui::UiState& uiState = ui.state();
    Serial.printf("[LCD] avgFrameUs=%u avgFlushUs=%u fps=%u screen=%s menu=%d action=%s\n",
                  avgFrameUs, avgFlushUs, fps, screenName(uiState.screen), uiState.menuIndex,
                  menuName(uiState.menuIndex, pet.state().sleeping));
    renderDiag.frames = 0;
    renderDiag.frameUs = 0;
    renderDiag.flushUs = 0;
    renderDiag.windowStartMs = nowMs;
}
#endif
}  // namespace

void setup() {
    platform.begin();
    if (!storage.begin()) {
        Serial.println("[Save] NVS init failed");
    }
    const auto boot = saves.boot();
    Serial.printf("[Save] namespace=neripal keys=slot0,slot1 boot=%s\n", bootLabel(boot));
    if (!wallClock.nowUnixSeconds().has_value()) {
        Serial.println("[Save] Wall-clock untrusted (year < 2020 or unset); snapshot kept, no offline catch-up");
    }
}

void loop() {
    while (const auto action = platform.pollAction()) {
        ui.handleInput(*action, pet.state());
    }
    if (platform.consumeScreenLock()) {
        // Stamp before sleep so the boot policy's gap is only the locked interval.
        saves.accountWallClockNow();
        const std::uint64_t sleptMs = platform.sleepUntilButtonWake();
        if (!saves.catchUpAccountedWallClock()) {
            pet.applyOffline(sleptMs, sleptMs);
        }
    }
    if (ui.takeEvolutionConfirm()) {
        pet.confirmEvolutionNotice();
    }
    if (!ui.showingEvolutionNotice()) {
        neripal::core::EvolutionNotice notice;
        if (pet.peekEvolutionNotice(notice)) {
            ui.presentEvolutionNotice(notice);
#if NERIPAL_RENDER_DIAGNOSTICS
            notePresentedNotice(notice);
#endif
        }
    }
    if (const auto care = ui.takeCareAction()) {
        const auto result = pet.apply(*care);
        saves.noteCareResult(result);
        ui.beginCareFeedback(*care, result, gameClock.nowMillis());
    }
    urgentSoundCue = neripal::ui::urgentSoundRequested(pet.state());
    (void)urgentSoundCue;
    pet.update();
    ui.update(gameClock.nowMillis());
    saves.tick();
#if NERIPAL_RENDER_DIAGNOSTICS
    const std::uint32_t renderStartUs = micros();
#endif
    view.render(platform, pet.state(), ui.state());
#if NERIPAL_RENDER_DIAGNOSTICS
    noteRenderSample(micros() - renderStartUs, platform.lastFlushMicros());
#endif
    delay(33);
}
