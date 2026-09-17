#include "WaveshareS3Platform.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/ui/PetView.hpp"
#include "neripal/ui/UiController.hpp"

#include <Arduino.h>

namespace {
neripal::waveshare_s3::Esp32Clock clockSource;
neripal::core::Pet pet(clockSource);
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
    pet.update();
    ui.update(clockSource.nowMillis());
    view.render(platform, pet.state(), ui.state());
    delay(33);
}
