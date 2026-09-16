#pragma once

#include "neripal/platform/IInput.hpp"
#include "neripal/platform/IRenderer.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace neripal::desktop {

class DesktopPlatform final : public platform::IRenderer, public platform::IInput {
public:
    explicit DesktopPlatform(HINSTANCE instance);
    ~DesktopPlatform() override;

    bool valid() const noexcept { return window_ != nullptr; }
    bool running() const noexcept { return running_; }
    void pumpEvents();
    void waitForNextFrame() const;
    std::optional<int> pollKey();
    std::optional<platform::InputAction> pollAction() override;

    void beginFrame(platform::Color color) override;
    void fillRect(int x, int y, int width, int height, platform::Color color) override;
    void drawRect(int x, int y, int width, int height, platform::Color color) override;
    void drawText(int x, int y, std::string_view text, platform::Color color, int scale) override;
    void endFrame() override;

private:
    enum class CommandType { Fill, Outline, Text };
    struct DrawCommand {
        CommandType type{};
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int textScale = 1;
        platform::Color color = 0;
        std::string text;
    };

    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    void paint(HDC dc, const RECT& client);
    static COLORREF toColorRef(platform::Color color);

    HWND window_ = nullptr;
    bool running_ = true;
    platform::Color background_ = 0;
    std::vector<DrawCommand> commands_;
    std::deque<int> keys_;
};

}  // namespace neripal::desktop
