#include "WaveshareS3Platform.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/core/XorShift32.hpp"
#include "neripal/ui/PetView.hpp"
#include "neripal/ui/UiController.hpp"

#include <Arduino.h>
#include <cstdint>

namespace {
// 0.4 placeholder until a platform entropy source exists (ADC, esp_random, or NVS).
// Chosen by this composition root; not a firmware product contract and not seed 1.
constexpr std::uint32_t kFirmwareRngSeedPlaceholder = 0x57415645u;  // 'WAVE'
neripal::waveshare_s3::Esp32Clock clockSource;
neripal::core::XorShift32 rng(kFirmwareRngSeedPlaceholder);
neripal::core::Pet pet(clockSource, rng);
neripal::waveshare_s3::WaveshareS3Platform platform;
neripal::ui::PetView view;
neripal::ui::UiController ui;
}

void setup() {
    platform.begin();
}

void loop() {
    while (const auto action = platform.pollAction()) {
        ui.handleInput(*action, pet.state());
    }
    if (const auto care = ui.takeCareAction()) {
        const auto result = pet.apply(*care);
        ui.beginCareFeedback(*care, result, clockSource.nowMillis());
    }
    pet.update();
    ui.update(clockSource.nowMillis());
    view.render(platform, pet.state(), ui.state());
    delay(33);
}
