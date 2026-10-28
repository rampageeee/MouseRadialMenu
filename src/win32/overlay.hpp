#pragma once
#include "platform.hpp"
#include <d2d1.h>
#include <dwrite.h>

namespace radial::win {
class Overlay {
public:
    Overlay() = default;
    ~Overlay();
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    void create(HINSTANCE instance);
    void place(POINT origin);
    void render(const MenuState& state, const Config& config);
    void hide();
    void relative(POINT pointer, double& x, double& y) const;
private:
    HWND window_{};
    ID2D1Factory* factory_{};
    IDWriteFactory* text_factory_{};
    ID2D1DCRenderTarget* target_{};
    IDWriteTextFormat* format_{};
    IDWriteTextFormat* small_{};
    HDC dc_{};
    HBITMAP bitmap_{};
    HGDIOBJ previous_{};
    POINT position_{};
    int pixels_ = 440;
    float scale_ = 1;
};
}
