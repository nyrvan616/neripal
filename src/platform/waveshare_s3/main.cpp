#include "Esp32WallClock.hpp"
#include "NvsSaveStorage.hpp"
#include "WaveshareS3Platform.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/core/XorShift32.hpp"
#include "neripal/persist/SaveSession.hpp"
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
    if (const auto care = ui.takeCareAction()) {
        const auto result = pet.apply(*care);
        saves.noteCareResult(result);
        ui.beginCareFeedback(*care, result, gameClock.nowMillis());
    }
    pet.update();
    ui.update(gameClock.nowMillis());
    saves.tick();
    view.render(platform, pet.state(), ui.state());
    delay(33);
}
