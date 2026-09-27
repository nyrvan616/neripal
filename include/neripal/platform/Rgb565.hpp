#pragma once

#include "neripal/platform/IRenderer.hpp"

namespace neripal::platform {

// 0x00RRGGBB -> RGB565, keeping the top bits of each channel.
// Header-only and constexpr so host tests can call it without Arduino.
constexpr std::uint16_t toRgb565(Color color) noexcept {
    const std::uint32_t red = (color >> 16) & 0xFFu;
    const std::uint32_t green = (color >> 8) & 0xFFu;
    const std::uint32_t blue = color & 0xFFu;
    return static_cast<std::uint16_t>(((red & 0xF8u) << 8) | ((green & 0xFCu) << 3) |
                                      (blue >> 3));
}

}  // namespace neripal::platform
