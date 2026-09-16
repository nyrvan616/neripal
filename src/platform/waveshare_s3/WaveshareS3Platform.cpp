#include "WaveshareS3Platform.hpp"

#include "neripal/Version.hpp"

#include <Arduino.h>
#include <esp_timer.h>

namespace neripal::waveshare_s3 {
namespace pins {
constexpr int kBatteryEnable = 2;
constexpr int kButton1 = 0;
constexpr int kButton2 = 5;
constexpr int kButton3 = 4;
constexpr int kBacklight = 46;
}

std::uint64_t Esp32Clock::nowMillis() const {
    return static_cast<std::uint64_t>(esp_timer_get_time()) / 1000ULL;
}

bool WaveshareS3Platform::begin() {
    Serial.begin(115200);
    pinMode(pins::kBatteryEnable, OUTPUT);
    digitalWrite(pins::kBatteryEnable, HIGH);
    pinMode(pins::kBacklight, OUTPUT);
    digitalWrite(pins::kBacklight, HIGH);
    pinMode(pins::kButton1, INPUT_PULLUP);
    pinMode(pins::kButton2, INPUT_PULLUP);
    pinMode(pins::kButton3, INPUT_PULLUP);
    Serial.printf("%s | WaveshareS3Platform\n", version::kDisplayName.data());
    Serial.println("HAL stub ready (display/touch pending)");
    return true;
}

std::optional<platform::InputAction> WaveshareS3Platform::pollAction() {
    const bool button1 = digitalRead(pins::kButton1) == LOW;
    const bool button2 = digitalRead(pins::kButton2) == LOW;
    const bool button3 = digitalRead(pins::kButton3) == LOW;
    std::optional<platform::InputAction> action;
    if (button1 && !lastButton1_) action = platform::InputAction::Next;
    else if (button2 && !lastButton2_) action = platform::InputAction::Confirm;
    else if (button3 && !lastButton3_) action = platform::InputAction::Back;
    lastButton1_ = button1;
    lastButton2_ = button2;
    lastButton3_ = button3;
    return action;
}

// The rendering contract is present now; ST7789 transport is intentionally deferred.
void WaveshareS3Platform::beginFrame(platform::Color) {}
void WaveshareS3Platform::fillRect(int, int, int, int, platform::Color) {}
void WaveshareS3Platform::drawRect(int, int, int, int, platform::Color) {}
void WaveshareS3Platform::drawText(int, int, std::string_view, platform::Color, int) {}
void WaveshareS3Platform::endFrame() {}

}  // namespace neripal::waveshare_s3
