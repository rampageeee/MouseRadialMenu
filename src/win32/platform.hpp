#pragma once
#include "core/core.hpp"
#include <windows.h>
#include <string>

namespace radial::win {
std::wstring wide(const std::string& utf8);
std::string utf8(const std::wstring& text);
struct Target { HWND window{}; DWORD pid{}; std::string executable; };
Target capture_target();
bool target_valid(const Target& target);
bool keys_released(const Chord& action, const Chord& activation);
// Hide the overlay and release menu controls before dispatch. A hold observer may
// remain installed; it must ignore injected input and pass unrelated physical keys.
void dispatch(const Target& target, const Chord& chord);
std::filesystem::path executable_directory();
}
