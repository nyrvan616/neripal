#include "WaveshareS3Platform.hpp"

#include "neripal/Version.hpp"
#include "neripal/platform/Rgb565.hpp"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <soc/soc_memory_types.h>

namespace neripal::waveshare_s3 {
namespace pins {
constexpr int kBatteryEnable = 2;
constexpr int kButton1 = 0;
constexpr int kButton2 = 5;
constexpr int kButton3 = 4;
constexpr int kBacklight = 46;
// ST7789 on SPI2. Same pins as the Waveshare Arduino examples and
// NeriHardwareTest DisplayTest: DC, CS, SCK, MOSI, no MISO.
constexpr int kLcdDc = 45;
constexpr int kLcdCs = 21;
constexpr int kLcdSck = 38;
constexpr int kLcdMosi = 39;
constexpr int kLcdMiso = -1;
constexpr int kLcdRst = 40;
constexpr int kLcdWidth = 240;
constexpr int kLcdHeight = 240;
constexpr int kLcdRotation = 0;
constexpr bool kLcdIps = true;
constexpr std::uint32_t kSerialWaitMs = 1000;
}

std::uint64_t Esp32Clock::nowMillis() const {
    return static_cast<std::uint64_t>(esp_timer_get_time()) / 1000ULL;
}

void WaveshareS3Platform::armButton(DebouncedButton& button, int pin,
                                   platform::InputAction action) {
    pinMode(pin, INPUT_PULLUP);
    button.pin = pin;
    button.action = action;
    button.raw = digitalRead(pin) == LOW;
    button.stable = button.raw;
    button.lastChangeMs = millis();
}

void WaveshareS3Platform::logFramebufferAudit(const void* framebuffer) const {
    const char* region = "NONE";
    if (framebuffer != nullptr) {
        region = esp_ptr_external_ram(framebuffer) ? "PSRAM" : "INTERNAL";
    }
    const std::size_t psramTotal = ESP.getPsramSize();
    const std::size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const std::size_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const std::size_t internalLargest =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    Serial.printf("[LCD] framebuffer ptr=%p bytes=%u region=%s\n", framebuffer,
                  static_cast<unsigned>(kFramebufferBytes), region);
    Serial.printf("[LCD] psram total=%u free=%u\n", static_cast<unsigned>(psramTotal),
                  static_cast<unsigned>(psramFree));
    Serial.printf("[LCD] internal free=%u largest=%u margin=%u\n",
                  static_cast<unsigned>(internalFree), static_cast<unsigned>(internalLargest),
                  static_cast<unsigned>(kInternalHeapSafetyMargin));
}

void WaveshareS3Platform::rejectDisplay(const char* line) {
    displayReady_ = false;
    digitalWrite(pins::kBacklight, LOW);
    Serial.println(line);
}

bool WaveshareS3Platform::begin() {
    static_assert(kFramebufferBytes ==
                      static_cast<std::size_t>(pins::kLcdWidth) * pins::kLcdHeight * 2,
                  "framebuffer byte count must match the 240x240 RGB565 canvas");

    pinMode(pins::kBatteryEnable, OUTPUT);
    digitalWrite(pins::kBatteryEnable, HIGH);

    Serial.begin(115200);
    const std::uint32_t serialWaitStart = millis();
    while (!Serial && (millis() - serialWaitStart) < pins::kSerialWaitMs) {
    }

    armButton(buttons_[0], pins::kButton1, platform::InputAction::Next);
    armButton(buttons_[1], pins::kButton2, platform::InputAction::Confirm);
    armButton(buttons_[2], pins::kButton3, platform::InputAction::Back);

    pinMode(pins::kBacklight, OUTPUT);
    digitalWrite(pins::kBacklight, LOW);

    Serial.printf("%s | WaveshareS3Platform\n", version::kDisplayName.data());

    // SPI clock stays at the library default (40 MHz on ESP32). No DMA bus.
    bus_ = new Arduino_ESP32SPI(pins::kLcdDc, pins::kLcdCs, pins::kLcdSck, pins::kLcdMosi,
                                pins::kLcdMiso);
    panel_ = new Arduino_ST7789(bus_, pins::kLcdRst, pins::kLcdRotation, pins::kLcdIps,
                                pins::kLcdWidth, pins::kLcdHeight);
    canvas_ = new Arduino_Canvas(pins::kLcdWidth, pins::kLcdHeight, panel_);
    if (bus_ == nullptr || panel_ == nullptr || canvas_ == nullptr || !canvas_->begin()) {
        logFramebufferAudit(canvas_ != nullptr ? canvas_->getFramebuffer() : nullptr);
        rejectDisplay("[LCD] INIT FAILED");
        return true;
    }

    const std::uint16_t* framebuffer = canvas_->getFramebuffer();
    logFramebufferAudit(framebuffer);
    const std::size_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const bool inPsram = framebuffer != nullptr && esp_ptr_external_ram(framebuffer);
    // Checked once, after the canvas allocation and before the normal loop.
    // PSRAM being tight is only logged above; it does not change strategy.
    // Internal placement with the margin still held is also only logged.
    if (framebuffer == nullptr) {
        rejectDisplay("[LCD] FRAMEBUFFER UNSAFE reason=null");
        return true;
    }
    if (internalFree < kInternalHeapSafetyMargin) {
        rejectDisplay(inPsram ? "[LCD] FRAMEBUFFER UNSAFE reason=internal_heap"
                              : "[LCD] FRAMEBUFFER UNSAFE reason=internal_ram");
        return true;
    }

    canvas_->setTextWrap(false);
    canvas_->fillScreen(0x0000);
    canvas_->flush();
    digitalWrite(pins::kBacklight, HIGH);
    displayReady_ = true;
    Serial.println("[LCD] INIT OK");
    return true;
}

std::optional<platform::InputAction> WaveshareS3Platform::pollAction() {
    const std::uint32_t nowMs = millis();
    std::optional<platform::InputAction> emitted;
    for (DebouncedButton& button : buttons_) {
        const bool pressed = digitalRead(button.pin) == LOW;
        if (pressed != button.raw) {
            button.raw = pressed;
            button.lastChangeMs = nowMs;
        }
        if ((nowMs - button.lastChangeMs) < kButtonDebounceMs) {
            continue;
        }
        if (pressed != button.stable) {
            // One action per call. A second stable edge waits for the next poll.
            if (emitted.has_value()) continue;
            button.stable = pressed;
            if (button.action == platform::InputAction::Confirm) {
                // Confirm is the release of a short press. A hold locks instead.
                // GPIO0 stays a press edge: it is a boot strap, not the lock.
                if (pressed) {
                    button.pressedAtMs = nowMs;
                    button.longPressConsumed = false;
                    button.suppressRelease = false;
                } else if (button.longPressConsumed || button.suppressRelease) {
                    button.longPressConsumed = false;
                    button.suppressRelease = false;
                } else if ((nowMs - button.pressedAtMs) >= kLongPressMs) {
                    button.longPressConsumed = true;
                    lockRequested_ = true;
                } else {
                    emitted = button.action;
                }
            } else if (pressed) {
                emitted = button.action;
            }
            continue;
        }
        if (pressed && button.action == platform::InputAction::Confirm &&
            !button.longPressConsumed && (nowMs - button.pressedAtMs) >= kLongPressMs) {
            button.longPressConsumed = true;
            lockRequested_ = true;
        }
    }
    return emitted;
}

bool WaveshareS3Platform::consumeScreenLock() noexcept {
    if (!lockRequested_) return false;
    lockRequested_ = false;
    return true;
}

void WaveshareS3Platform::waitForConfirmRelease() {
    DebouncedButton* confirm = nullptr;
    for (DebouncedButton& button : buttons_) {
        if (button.action == platform::InputAction::Confirm) {
            confirm = &button;
            break;
        }
    }
    if (confirm == nullptr) return;

    while (true) {
        const std::uint32_t nowMs = millis();
        const bool pressed = digitalRead(confirm->pin) == LOW;
        if (pressed != confirm->raw) {
            confirm->raw = pressed;
            confirm->lastChangeMs = nowMs;
        }
        if (!pressed && (nowMs - confirm->lastChangeMs) >= kButtonDebounceMs) {
            confirm->stable = false;
            confirm->longPressConsumed = false;
            confirm->suppressRelease = false;
            return;
        }
        delay(1);
    }
}

void WaveshareS3Platform::holdSleepPads() {
    // GPIO2 must stay HIGH or a battery-powered board switches off. Holding the
    // pad keeps that level through light sleep. This is not a power-off.
    digitalWrite(pins::kBacklight, LOW);
    const esp_err_t backlight = gpio_hold_en(static_cast<gpio_num_t>(pins::kBacklight));
    const esp_err_t power = gpio_hold_en(static_cast<gpio_num_t>(pins::kBatteryEnable));
    if (backlight != ESP_OK || power != ESP_OK) {
        Serial.println("[LCD] pad hold failed");
    }
}

void WaveshareS3Platform::releaseSleepPads() {
    gpio_hold_dis(static_cast<gpio_num_t>(pins::kBacklight));
    gpio_hold_dis(static_cast<gpio_num_t>(pins::kBatteryEnable));
}

void WaveshareS3Platform::armButtonWake() {
    const gpio_num_t wakePins[] = {
        static_cast<gpio_num_t>(pins::kButton1),
        static_cast<gpio_num_t>(pins::kButton2),
        static_cast<gpio_num_t>(pins::kButton3),
    };
    for (const gpio_num_t pin : wakePins) {
        gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
    }
    esp_sleep_enable_gpio_wakeup();
}

void WaveshareS3Platform::disarmButtonWake() {
    const gpio_num_t wakePins[] = {
        static_cast<gpio_num_t>(pins::kButton1),
        static_cast<gpio_num_t>(pins::kButton2),
        static_cast<gpio_num_t>(pins::kButton3),
    };
    for (const gpio_num_t pin : wakePins) {
        gpio_wakeup_disable(pin);
    }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
}

void WaveshareS3Platform::swallowWakeEdges() {
    const std::uint32_t nowMs = millis();
    for (DebouncedButton& button : buttons_) {
        const bool pressed = digitalRead(button.pin) == LOW;
        button.raw = pressed;
        button.stable = pressed;
        button.lastChangeMs = nowMs;
        if (button.action != platform::InputAction::Confirm) continue;
        button.pressedAtMs = nowMs;
        // The press that woke the chip is not Confirm and does not lock again.
        button.longPressConsumed = pressed;
        button.suppressRelease = pressed;
    }
}

std::uint64_t WaveshareS3Platform::sleepUntilButtonWake() {
    flushSuppressed_ = true;
    digitalWrite(pins::kBacklight, LOW);
    Serial.println("[LCD] lock");
    const std::int64_t beforeUs = esp_timer_get_time();
    waitForConfirmRelease();
    holdSleepPads();
    armButtonWake();
    const esp_err_t slept = esp_light_sleep_start();
    const std::int64_t afterUs = esp_timer_get_time();
    disarmButtonWake();
    releaseSleepPads();
    swallowWakeEdges();
    flushSuppressed_ = false;
    if (displayReady_) {
        backlightAfterFlush_ = true;
    }
    std::uint64_t sleptMs = 0;
    if (afterUs > beforeUs) {
        sleptMs = static_cast<std::uint64_t>(afterUs - beforeUs) / 1000ULL;
    }
    if (slept != ESP_OK) {
        Serial.printf("[LCD] light sleep failed err=%d\n", static_cast<int>(slept));
    }
    Serial.printf("[LCD] wake sleptMs=%llu\n", static_cast<unsigned long long>(sleptMs));
    return sleptMs;
}

// Draws land in the canvas framebuffer. endFrame() is the only flush.
// drawText uses the built-in glcdfont: 6x8 cells, cursor at the top-left.
void WaveshareS3Platform::beginFrame(platform::Color color) {
    if (!displayReady_) return;
    canvas_->fillScreen(platform::toRgb565(color));
}

void WaveshareS3Platform::fillRect(int x, int y, int width, int height, platform::Color color) {
    if (!displayReady_) return;
    canvas_->fillRect(static_cast<std::int16_t>(x), static_cast<std::int16_t>(y),
                      static_cast<std::int16_t>(width), static_cast<std::int16_t>(height),
                      platform::toRgb565(color));
}

void WaveshareS3Platform::drawRect(int x, int y, int width, int height, platform::Color color) {
    if (!displayReady_) return;
    canvas_->drawRect(static_cast<std::int16_t>(x), static_cast<std::int16_t>(y),
                      static_cast<std::int16_t>(width), static_cast<std::int16_t>(height),
                      platform::toRgb565(color));
}

void WaveshareS3Platform::drawText(int x, int y, std::string_view text,
                                   platform::Color color, int scale) {
    if (!displayReady_) return;
    const int textScale = scale < 1 ? 1 : scale;
    canvas_->setTextSize(static_cast<std::uint8_t>(textScale));
    canvas_->setTextColor(platform::toRgb565(color));
    canvas_->setCursor(static_cast<std::int16_t>(x), static_cast<std::int16_t>(y));
    for (const unsigned char character : text) {
        canvas_->write(character);
    }
}

void WaveshareS3Platform::endFrame() {
    if (!displayReady_ || flushSuppressed_) {
#if NERIPAL_RENDER_DIAGNOSTICS
        lastFlushUs_ = 0;
#endif
        return;
    }
#if NERIPAL_RENDER_DIAGNOSTICS
    const std::uint32_t started = micros();
    canvas_->flush();
    lastFlushUs_ = micros() - started;
#else
    canvas_->flush();
#endif
    if (backlightAfterFlush_) {
        digitalWrite(pins::kBacklight, HIGH);
        backlightAfterFlush_ = false;
    }
}

}  // namespace neripal::waveshare_s3
