#include "overlay.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace radial::win {
namespace {
template<class T> void release(T*& p) { if (p) { p->Release(); p = nullptr; } }
void check(HRESULT result, const char* message) { if (FAILED(result)) throw std::runtime_error(message); }
LRESULT CALLBACK overlay_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATEANDEAT;
    if (message == WM_ERASEBKGND) return 1;
    return DefWindowProcW(window, message, w, l);
}
D2D1_POINT_2F point(double angle, float radius) {
    return D2D1::Point2F(220 + radius * static_cast<float>(std::sin(angle)), 220 - radius * static_cast<float>(std::cos(angle)));
}
}
Overlay::~Overlay() {
    if (window_) DestroyWindow(window_);
    if (dc_ && previous_) SelectObject(dc_, previous_);
    if (bitmap_) DeleteObject(bitmap_);
    if (dc_) DeleteDC(dc_);
    release(small_); release(format_); release(target_); release(text_factory_); release(factory_);
}
void Overlay::create(HINSTANCE instance) {
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpfnWndProc = overlay_proc;
    wc.lpszClassName = L"TinyRadialOverlay";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Cannot register overlay class");
    window_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
        wc.lpszClassName, L"TinyRadial", WS_POPUP, 0, 0, pixels_, pixels_, nullptr, nullptr, instance, nullptr);
    if (!window_) throw std::runtime_error("Cannot create non-activating overlay");
    check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory_), "Direct2D initialization failed");
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&text_factory_)), "DirectWrite initialization failed");
    auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    check(factory_->CreateDCRenderTarget(&properties, &target_), "Cannot create layered render target");
    target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    auto make_format = [&](float size, IDWriteTextFormat** out) {
        check(text_factory_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", out), "Cannot create text format");
        (*out)->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        (*out)->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        (*out)->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    };
    make_format(14, &format_); make_format(11, &small_);
    dc_ = CreateCompatibleDC(nullptr);
    if (!dc_) throw std::runtime_error("Cannot allocate overlay device context");
}
void Overlay::place(POINT origin) {
    MONITORINFO monitor{sizeof(MONITORINFO), {}, {}, 0};
    HMONITOR selected = MonitorFromPoint(origin, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfoW(selected, &monitor)) throw std::runtime_error("Cannot query monitor bounds");
    // Query monitor DPI without a shcore import; a hidden window is positioned on that monitor first.
    SetWindowPos(window_, nullptr, origin.x, origin.y, 1, 1, SWP_NOACTIVATE | SWP_NOZORDER);
    UINT dpi = GetDpiForWindow(window_);
    if (!dpi) dpi = 96;
    float desired = static_cast<float>(dpi) / 96;
    float fit = static_cast<float>(std::min(monitor.rcWork.right - monitor.rcWork.left, monitor.rcWork.bottom - monitor.rcWork.top)) / 440;
    scale_ = std::min(desired, fit);
    pixels_ = std::max(1, static_cast<int>(std::floor(440 * scale_)));
    scale_ = static_cast<float>(pixels_) / 440;
    position_.x = std::clamp(origin.x - pixels_ / 2, monitor.rcWork.left, monitor.rcWork.right - pixels_);
    position_.y = std::clamp(origin.y - pixels_ / 2, monitor.rcWork.top, monitor.rcWork.bottom - pixels_);
    if (bitmap_) {
        SelectObject(dc_, previous_); DeleteObject(bitmap_); bitmap_ = nullptr;
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = pixels_;
    info.bmiHeader.biHeight = -pixels_;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    bitmap_ = CreateDIBSection(dc_, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap_) throw std::runtime_error("Cannot allocate overlay bitmap");
    previous_ = SelectObject(dc_, bitmap_);
    RECT bounds{0, 0, pixels_, pixels_};
    check(target_->BindDC(dc_, &bounds), "Cannot bind overlay bitmap");
    target_->SetDpi(96, 96);
    target_->SetTransform(D2D1::Matrix3x2F::Scale(scale_, scale_));
}
void Overlay::render(const MenuState& state, const Config& config) {
    if (!state.active()) return;
    ID2D1SolidColorBrush* brush = nullptr;
    check(target_->CreateSolidColorBrush(D2D1::ColorF(0xffffff), &brush), "Cannot create overlay brush");
    target_->BeginDraw();
    target_->Clear(D2D1::ColorF(0, 0.0f));
    const auto& menu = state.menu();
    const double width = 2 * std::numbers::pi / static_cast<double>(menu.items.size());
    HRESULT geometry_result = S_OK;
    for (std::size_t i = 0; i < menu.items.size(); ++i) {
        ID2D1PathGeometry* geometry = nullptr;
        ID2D1GeometrySink* sink = nullptr;
        geometry_result = factory_->CreatePathGeometry(&geometry);
        if (SUCCEEDED(geometry_result)) geometry_result = geometry->Open(&sink);
        if (FAILED(geometry_result)) { release(sink); release(geometry); break; }
        sink->BeginFigure(D2D1::Point2F(220, 220), D2D1_FIGURE_BEGIN_FILLED);
        // Polyline arc handles one-item and two-item menus without degenerate arc endpoints.
        for (int step = 0; step <= 48; ++step) {
            const double angle = (static_cast<double>(i) - 0.5) * width + width * step / 48;
            sink->AddLine(point(angle, 200));
        }
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        geometry_result = sink->Close();
        if (SUCCEEDED(geometry_result)) {
            brush->SetColor(D2D1::ColorF(static_cast<int>(i) == state.selected() ? 0x255c83 : 0x202932, 0.97f));
            target_->FillGeometry(geometry, brush);
            brush->SetColor(D2D1::ColorF(0x425260));
            target_->DrawGeometry(geometry, brush, 1);
        }
        release(sink); release(geometry);
        if (FAILED(geometry_result)) break;
    }
    auto draw = [&](const std::string& label, D2D1_RECT_F rect, IDWriteTextFormat* format, UINT32 color) {
        auto s = wide(label);
        brush->SetColor(D2D1::ColorF(color));
        target_->DrawText(s.data(), static_cast<UINT32>(s.size()), format, rect, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    };
    for (std::size_t i = 0; i < menu.items.size(); ++i) {
        const auto& item = menu.items[i];
        const auto p = point(i * width, 140);
        draw(item.label + (item.submenu ? " >" : ""), D2D1::RectF(p.x - 49, p.y - 25, p.x + 49, p.y + 10), format_, 0xf2f5f7);
        if (config.show_shortcuts && item.chord)
            draw(item.chord->text, D2D1::RectF(p.x - 49, p.y + 10, p.x + 49, p.y + 28), small_, 0xb7cedd);
    }
    brush->SetColor(D2D1::ColorF(0x111a23));
    target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(220, 220), static_cast<float>(config.dead_zone), static_cast<float>(config.dead_zone)), brush);
    draw("Cancel", D2D1::RectF(194, 206, 246, 234), small_, 0xd7e0e8);
    draw(menu.label, D2D1::RectF(80, 0, 360, 19), small_, 0xf2f5f7);
    draw(state.nested() ? "Center to rearm | Right click / Backspace: back | Esc: cancel" : "Click / Enter / hotkey: select | Esc: cancel",
        D2D1::RectF(10, 420, 430, 439), small_, 0xd7e0e8);
    HRESULT result = target_->EndDraw();
    release(brush);
    check(geometry_result, "Cannot draw menu geometry");
    check(result, "Overlay drawing failed; close and restart TinyRadial");
    SIZE size{pixels_, pixels_};
    POINT source{0, 0};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    if (!UpdateLayeredWindow(window_, nullptr, &position_, &size, dc_, &source, 0, &blend, ULW_ALPHA))
        throw std::runtime_error("Cannot update transparent overlay");
    ShowWindow(window_, SW_SHOWNOACTIVATE);
}
void Overlay::hide() { if (window_) ShowWindow(window_, SW_HIDE); }
void Overlay::relative(POINT pointer, double& x, double& y) const {
    x = static_cast<double>(pointer.x - position_.x) / scale_ - 220;
    y = static_cast<double>(pointer.y - position_.y) / scale_ - 220;
}
}
