#pragma once

#include "neripal/core/IClock.hpp"
#include "neripal/platform/IInput.hpp"
#include "neripal/platform/IRenderer.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

class Arduino_DataBus;
class Arduino_GFX;
class Arduino_Canvas;

namespace neripal::waveshare_s3 {

// Unscaled monotonic clock from esp_timer. Pet (gameplay) and SaveSession (autosave)
// use separate instances; neither is a wall-clock and neither is scaled.
class Esp32Clock final : public core::IClock {
public:
    std::uint64_t nowMillis() const override;
};

class WaveshareS3Platform final : public platform::IRenderer, public platform::IInput {
public:
    bool begin();
    std::optional<platform::InputAction> pollAction() override;

    // True once after Confirm (GPIO5) has been held for kLongPressMs.
    // That hold does not emit Confirm. GPIO0 is not a lock trigger.
    bool consumeScreenLock() noexcept;

    // Backlight off and no flush. Light-sleeps until GPIO0, GPIO5 or GPIO4
    // reads low, then consumes that wake edge. The next endFrame() flushes
    // and only then turns the backlight on. Returns the blocked milliseconds,
    // including the release wait before sleep.
    std::uint64_t sleepUntilButtonWake();

    void beginFrame(platform::Color color) override;
    void fillRect(int x, int y, int width, int height, platform::Color color) override;
    void drawRect(int x, int y, int width, int height, platform::Color color) override;
    void drawText(int x, int y, std::string_view text, platform::Color color, int scale) override;
    void endFrame() override;

#if NERIPAL_RENDER_DIAGNOSTICS
    // Micros spent in the last canvas flush. Zero when the display is off.
    std::uint32_t lastFlushMicros() const noexcept { return lastFlushUs_; }
#endif

private:
    struct DebouncedButton {
        int pin = -1;
        platform::InputAction action = platform::InputAction::Next;
        bool raw = false;
        bool stable = false;
        std::uint32_t lastChangeMs = 0;
        std::uint32_t pressedAtMs = 0;
        bool longPressConsumed = false;
        bool suppressRelease = false;
    };

    void armButton(DebouncedButton& button, int pin, platform::InputAction action);
    void logFramebufferAudit(const void* framebuffer) const;
    void rejectDisplay(const char* line);
    void waitForConfirmRelease();
    void holdSleepPads();
    void releaseSleepPads();
    void armButtonWake();
    void disarmButtonWake();
    void swallowWakeEdges();

    // Same quiet time as NeriHardwareTest ButtonTest. Per pin, not a delay.
    static constexpr std::uint32_t kButtonDebounceMs = 40;
    // Hold of Confirm after it is debounced. A shorter release stays Confirm.
    static constexpr std::uint32_t kLongPressMs = 600;

    // Provisional 0.6.1 safety margin. Checked once in begin(), after the canvas
    // allocates its framebuffer and before the normal loop. Below this, the
    // display stays off. This check does not pick another rendering strategy.
    static constexpr std::size_t kInternalHeapSafetyMargin = 64 * 1024;
    // 240x240 RGB565. Must match the canvas created in begin().
    static constexpr std::size_t kFramebufferBytes = 240 * 240 * 2;

    // Arduino_GFX stays behind this translation unit. Null until begin().
    Arduino_DataBus* bus_ = nullptr;
    Arduino_GFX* panel_ = nullptr;
    Arduino_Canvas* canvas_ = nullptr;
    // False keeps every draw call a no-op. The game loop still runs.
    bool displayReady_ = false;
    // Set by a GPIO5 hold. Cleared by consumeScreenLock().
    bool lockRequested_ = false;
    // endFrame() skips the panel while the device is locked.
    bool flushSuppressed_ = false;
    // The first flush after wake turns the backlight on.
    bool backlightAfterFlush_ = false;

    // GPIO0 Next, GPIO5 Confirm, GPIO4 Back. Order is the poll priority.
    DebouncedButton buttons_[3]{};

#if NERIPAL_RENDER_DIAGNOSTICS
    std::uint32_t lastFlushUs_ = 0;
#endif
};

}  // namespace neripal::waveshare_s3
