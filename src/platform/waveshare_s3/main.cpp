#include "WaveshareS3Platform.hpp"
#include "neripal/core/Pet.hpp"
#include "neripal/ui/PetView.hpp"

#include <Arduino.h>

namespace {
neripal::waveshare_s3::Esp32Clock clockSource;
neripal::core::Pet pet(clockSource);
neripal::waveshare_s3::WaveshareS3Platform platform;
neripal::ui::PetView view;
}

void setup() {
    platform.begin();
}

void loop() {
    while (const auto action = platform.pollAction()) {
        switch (*action) {
            case neripal::platform::InputAction::Feed: pet.feed(); break;
            case neripal::platform::InputAction::Train: pet.train(); break;
            case neripal::platform::InputAction::Sleep:
                pet.state().sleeping ? pet.wake() : pet.sleep();
                break;
            case neripal::platform::InputAction::Wake: pet.wake(); break;
            case neripal::platform::InputAction::Reset: pet.reset(); break;
        }
    }
    pet.update();
    view.render(platform, pet.state());
    delay(33);
}
