#pragma once

#include <cstdint>
#include <string_view>

namespace neripal::platform {

using Color = std::uint32_t; // 0x00RRGGBB

class IRenderer {
public:
    static constexpr int kLogicalWidth = 240;
    static constexpr int kLogicalHeight = 240;

    virtual ~IRenderer() = default;
    virtual void beginFrame(Color color) = 0;
    virtual void fillRect(int x, int y, int width, int height, Color color) = 0;
    virtual void drawRect(int x, int y, int width, int height, Color color) = 0;
    virtual void drawText(int x, int y, std::string_view text, Color color, int scale = 1) = 0;
    virtual void endFrame() = 0;
};

}  // namespace neripal::platform
