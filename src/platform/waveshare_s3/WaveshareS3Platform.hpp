#pragma once

#include "neripal/core/IClock.hpp"
#include "neripal/platform/IInput.hpp"
#include "neripal/platform/IRenderer.hpp"

#include <optional>

namespace neripal::waveshare_s3 {

class Esp32Clock final : public core::IClock {
public:
    std::uint64_t nowMillis() const override;
};

class WaveshareS3Platform final : public platform::IRenderer, public platform::IInput {
public:
    bool begin();
    std::optional<platform::InputAction> pollAction() override;

    void beginFrame(platform::Color color) override;
    void fillRect(int x, int y, int width, int height, platform::Color color) override;
    void drawRect(int x, int y, int width, int height, platform::Color color) override;
    void drawText(int x, int y, std::string_view text, platform::Color color, int scale) override;
    void endFrame() override;

private:
    bool lastButton1_ = false;
    bool lastButton2_ = false;
    bool lastButton3_ = false;
};

}  // namespace neripal::waveshare_s3
