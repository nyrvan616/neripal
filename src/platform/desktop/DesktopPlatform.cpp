#include "DesktopPlatform.hpp"

#include "neripal/Version.hpp"

#include <algorithm>
#include <string>

namespace neripal::desktop {
namespace {
constexpr wchar_t kWindowClass[] = L"NeriPalSimulatorWindow";
constexpr int kWindowScale = 3;
}

DesktopPlatform::DesktopPlatform(HINSTANCE instance) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = &DesktopPlatform::windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassW(&wc);

    RECT requested{0, 0, IRenderer::kLogicalWidth * kWindowScale,
                         IRenderer::kLogicalHeight * kWindowScale};
    AdjustWindowRect(&requested, WS_OVERLAPPEDWINDOW, FALSE);
    window_ = CreateWindowExW(0, kWindowClass, L"NeriPal - Desktop Simulator",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, requested.right - requested.left,
        requested.bottom - requested.top, nullptr, nullptr, instance, this);
    if (window_) {
        const std::string title = std::string(version::kDisplayName) + " - Desktop Simulator";
        SetWindowTextA(window_, title.c_str());
        ShowWindow(window_, SW_SHOW);
        UpdateWindow(window_);
    } else {
        running_ = false;
    }
}

DesktopPlatform::~DesktopPlatform() {
    if (window_) DestroyWindow(window_);
}

COLORREF DesktopPlatform::toColorRef(platform::Color color) {
    return RGB((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
}

void DesktopPlatform::pumpEvents() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) running_ = false;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void DesktopPlatform::waitForNextFrame() const {
    Sleep(16);
}

std::optional<int> DesktopPlatform::pollKey() {
    if (keys_.empty()) return std::nullopt;
    const int key = keys_.front();
    keys_.pop_front();
    return key;
}

std::optional<platform::InputAction> DesktopPlatform::pollAction() {
    const auto key = pollKey();
    if (!key) return std::nullopt;
    switch (*key) {
        case 'F': return platform::InputAction::Feed;
        case 'T': return platform::InputAction::Train;
        case 'S': return platform::InputAction::Sleep;
        case 'W': return platform::InputAction::Wake;
        case 'R': return platform::InputAction::Reset;
        default: return std::nullopt;
    }
}

void DesktopPlatform::beginFrame(platform::Color color) {
    background_ = color;
    commands_.clear();
}

void DesktopPlatform::fillRect(int x, int y, int width, int height, platform::Color color) {
    commands_.push_back({CommandType::Fill, x, y, width, height, 1, color, {}});
}

void DesktopPlatform::drawRect(int x, int y, int width, int height, platform::Color color) {
    commands_.push_back({CommandType::Outline, x, y, width, height, 1, color, {}});
}

void DesktopPlatform::drawText(int x, int y, std::string_view text,
                               platform::Color color, int scale) {
    DrawCommand command{CommandType::Text, x, y, 0, 0, std::max(1, scale), color, {}};
    command.text.assign(text.begin(), text.end());
    commands_.push_back(std::move(command));
}

void DesktopPlatform::endFrame() {
    if (window_) InvalidateRect(window_, nullptr, FALSE);
}

LRESULT CALLBACK DesktopPlatform::windowProc(HWND window, UINT message,
                                              WPARAM wParam, LPARAM lParam) {
    DesktopPlatform* self = reinterpret_cast<DesktopPlatform*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<DesktopPlatform*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->handleMessage(window, message, wParam, lParam)
                : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT DesktopPlatform::handleMessage(HWND window, UINT message,
                                       WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_KEYDOWN:
            if ((lParam & (1LL << 30)) == 0) keys_.push_back(static_cast<int>(wParam));
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);
            RECT client{};
            GetClientRect(window, &client);
            paint(dc, client);
            EndPaint(window, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY:
            window_ = nullptr;
            running_ = false;
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}

void DesktopPlatform::paint(HDC dc, const RECT& client) {
    const int clientWidth = client.right - client.left;
    const int clientHeight = client.bottom - client.top;
    if (clientWidth <= 0 || clientHeight <= 0) return;

    HDC buffer = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, clientWidth, clientHeight);
    if (!buffer || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (buffer) DeleteDC(buffer);
        return;
    }
    HGDIOBJ previousBitmap = SelectObject(buffer, bitmap);

    const int scale = std::max(1, std::min(clientWidth / IRenderer::kLogicalWidth,
                                           clientHeight / IRenderer::kLogicalHeight));
    const int originX = (clientWidth - IRenderer::kLogicalWidth * scale) / 2;
    const int originY = (clientHeight - IRenderer::kLogicalHeight * scale) / 2;

    HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    FillRect(buffer, &client, black);
    RECT screen{originX, originY, originX + IRenderer::kLogicalWidth * scale,
                                  originY + IRenderer::kLogicalHeight * scale};
    HBRUSH background = CreateSolidBrush(toColorRef(background_));
    FillRect(buffer, &screen, background);
    DeleteObject(background);
    SetBkMode(buffer, TRANSPARENT);

    for (const auto& command : commands_) {
        const int x = originX + command.x * scale;
        const int y = originY + command.y * scale;
        if (command.type == CommandType::Text) {
            const int fontHeight = 8 * scale * command.textScale;
            HFONT font = CreateFontA(fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Terminal");
            HGDIOBJ previous = SelectObject(buffer, font);
            SetTextColor(buffer, toColorRef(command.color));
            TextOutA(buffer, x, y, command.text.c_str(), static_cast<int>(command.text.size()));
            SelectObject(buffer, previous);
            DeleteObject(font);
            continue;
        }

        RECT rect{x, y, x + command.width * scale, y + command.height * scale};
        if (command.type == CommandType::Fill) {
            HBRUSH brush = CreateSolidBrush(toColorRef(command.color));
            FillRect(buffer, &rect, brush);
            DeleteObject(brush);
        } else {
            HPEN pen = CreatePen(PS_SOLID, scale, toColorRef(command.color));
            HGDIOBJ oldPen = SelectObject(buffer, pen);
            HGDIOBJ oldBrush = SelectObject(buffer, GetStockObject(NULL_BRUSH));
            Rectangle(buffer, rect.left, rect.top, rect.right, rect.bottom);
            SelectObject(buffer, oldBrush);
            SelectObject(buffer, oldPen);
            DeleteObject(pen);
        }
    }

    BitBlt(dc, 0, 0, clientWidth, clientHeight, buffer, 0, 0, SRCCOPY);
    SelectObject(buffer, previousBitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
}

}  // namespace neripal::desktop
