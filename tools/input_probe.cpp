// TinyRadial input observer. No injection, suppression, clipboard use, or disk log.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <array>
#include <deque>
#include <sstream>
#include <string>

namespace {
constexpr UINT keyMessage = WM_APP + 20;
constexpr UINT foregroundMessage = WM_APP + 21;
constexpr UINT_PTR refreshTimer = 1;
constexpr int observeButton = 101;
constexpr int clearButton = 102;
HWND mainWindow{};
HWND observeControl{};
HHOOK keyboardHook{};
HWINEVENTHOOK foregroundHook{};
bool observing{};
std::deque<std::wstring> lines;
LONGLONG rawX{}, rawY{};
unsigned long long rawPackets{};

void log(const std::wstring& value) {
    std::wostringstream out;
    out << GetTickCount64() << L" ms  " << value;
    lines.push_back(out.str());
    if (lines.size() > 350) lines.pop_front();
    InvalidateRect(mainWindow, nullptr, FALSE);
}

std::wstring hex(unsigned long long value) {
    std::wostringstream out;
    out << L"0x" << std::hex << value;
    return out.str();
}

LRESULT CALLBACK keyboard(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION && observing && GetForegroundWindow() == mainWindow) {
        const auto& event = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        const auto direction = (message == WM_KEYUP || message == WM_SYSKEYUP) ? 0x10000u : 0u;
        // Keep the hook fast; text formatting belongs to the normal message loop.
        PostMessageW(mainWindow, keyMessage, event.vkCode | direction, event.flags);
    }
    return CallNextHookEx(keyboardHook, code, message, data);
}

void updateKeyboardHook() {
    const bool wanted = observing && GetForegroundWindow() == mainWindow;
    if (!wanted && keyboardHook) {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = nullptr;
    } else if (wanted && !keyboardHook) {
        keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard, GetModuleHandleW(nullptr), 0);
        if (!keyboardHook) log(L"ERROR installing observer hook: " + std::to_wstring(GetLastError()));
    }
}

void CALLBACK foreground(HWINEVENTHOOK, DWORD, HWND window, LONG, LONG, DWORD, DWORD) {
    PostMessageW(mainWindow, foregroundMessage, reinterpret_cast<WPARAM>(window), 0);
}

void stop() {
    observing = false;
    updateKeyboardHook();
    if (foregroundHook) UnhookWinEvent(foregroundHook);
    foregroundHook = nullptr;
    RAWINPUTDEVICE device{0x01, 0x02, RIDEV_REMOVE, nullptr};
    RegisterRawInputDevices(&device, 1, sizeof(device));
    KillTimer(mainWindow, refreshTimer);
    SetWindowTextW(observeControl, L"Start observation");
}

void start() {
    RAWINPUTDEVICE device{0x01, 0x02, RIDEV_INPUTSINK, mainWindow};
    if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
        log(L"ERROR registering Raw Input: " + std::to_wstring(GetLastError()));
        return;
    }
    foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, foreground, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!foregroundHook) {
        log(L"ERROR installing foreground observer: " + std::to_wstring(GetLastError()));
        stop();
        return;
    }
    observing = true;
    SetTimer(mainWindow, refreshTimer, 250, nullptr);
    SetWindowTextW(observeControl, L"Stop observation");
    updateKeyboardHook();
    log(L"Observing. Keyboard hook runs only while this window is foreground.");
    log(L"Raw mouse packets are counted in background too; this does not block other apps.");
}

void rawInput(HRAWINPUT handle) {
    RAWINPUT input{};
    UINT bytes = sizeof(input);
    const auto result = GetRawInputData(handle, RID_INPUT, &input, &bytes, sizeof(RAWINPUTHEADER));
    if (result == static_cast<UINT>(-1) || input.header.dwType != RIM_TYPEMOUSE) return;
    ++rawPackets;
    if (!(input.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
        rawX += input.data.mouse.lLastX;
        rawY += input.data.mouse.lLastY;
    }
    if (input.data.mouse.usButtonFlags) {
        log(L"RAW mouse button flags=" + hex(input.data.mouse.usButtonFlags));
    }
}

void paint(HWND window) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(window, &ps);
    RECT bounds{};
    GetClientRect(window, &bounds);
    FillRect(dc, &bounds, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
    SetBkMode(dc, TRANSPARENT);
    auto oldFont = SelectObject(dc, GetStockObject(ANSI_FIXED_FONT));
    std::wstring status = L"Raw packets: " + std::to_wstring(rawPackets) +
        L"  relative dx/dy totals: " + std::to_wstring(rawX) + L" / " + std::to_wstring(rawY);
    TextOutW(dc, 10, 52, status.c_str(), static_cast<int>(status.size()));
    const int visible = std::max(0L, (bounds.bottom - 86L) / 18L);
    const auto first = lines.size() > static_cast<size_t>(visible) ? lines.size() - visible : 0;
    int y = 78;
    for (size_t i = first; i < lines.size(); ++i, y += 18) {
        TextOutW(dc, 10, y, lines[i].c_str(), static_cast<int>(lines[i].size()));
    }
    SelectObject(dc, oldFont);
    EndPaint(window, &ps);
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        mainWindow = window;
        observeControl = CreateWindowW(L"BUTTON", L"Start observation", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 10, 180, 30, window, reinterpret_cast<HMENU>(observeButton), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            200, 10, 80, 30, window, reinterpret_cast<HMENU>(clearButton), nullptr, nullptr);
        log(L"Safe target: no editable document, no saved log. Click Start observation.");
        log(L"VK codes: F13=0x7c, Ctrl=0x11, Alt=0x12, Shift=0x10, Space=0x20.");
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == observeButton) {
            SetFocus(window);
            if (observing) { stop(); log(L"Stopped; observer hooks and Raw Input registration removed."); }
            else start();
        } else if (LOWORD(wParam) == clearButton) {
            lines.clear(); rawX = rawY = 0; rawPackets = 0;
            SetFocus(window);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_ACTIVATE:
        updateKeyboardHook();
        if (observing) log(LOWORD(wParam) == WA_INACTIVE ? L"Probe deactivated." : L"Probe activated.");
        return 0;
    case foregroundMessage: {
        updateKeyboardHook();
        if (!observing) return 0;
        DWORD pid{};
        GetWindowThreadProcessId(reinterpret_cast<HWND>(wParam), &pid);
        log(L"Foreground HWND=" + hex(wParam) + L" PID=" + std::to_wstring(pid) +
            (reinterpret_cast<HWND>(wParam) == window ? L" (probe)" : L" (other window)"));
        return 0;
    }
    case keyMessage:
        if (observing) log(L"HOOK " + std::wstring(wParam & 0x10000u ? L"UP   " : L"DOWN ") +
            L"VK=" + hex(wParam & 0xffffu) + L" injected=" +
            ((lParam & LLKHF_INJECTED) ? L"yes" : L"no") +
            L" flags=" + hex(static_cast<unsigned long long>(lParam)));
        return 0;
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        if (observing) log(L"WINDOW " + std::wstring(message == WM_KEYUP || message == WM_SYSKEYUP ? L"UP   " : L"DOWN ") +
            L"VK=" + hex(wParam) + L" repeat=" + std::to_wstring(lParam & 0xffff));
        break; // Preserve ordinary default processing: observer never consumes input.
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
        SetFocus(window);
        if (observing) log(L"WINDOW mouse message=" + hex(message));
        break;
    case WM_INPUT:
        if (observing) rawInput(reinterpret_cast<HRAWINPUT>(lParam));
        // DefWindowProc performs the required foreground WM_INPUT cleanup.
        break;
    case WM_TIMER:
        if (wParam == refreshTimer) InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_PAINT:
        paint(window); return 0;
    case WM_DESTROY:
        stop(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW type{};
    type.lpfnWndProc = windowProcedure;
    type.hInstance = instance;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    type.lpszClassName = L"TinyRadialInputProbe";
    if (!RegisterClassW(&type)) return 1;
    HWND window = CreateWindowW(type.lpszClassName, L"TinyRadial InputProbe - observation only",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1100, 720,
        nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    MSG message{};
    BOOL result{};
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return result < 0 ? 1 : static_cast<int>(message.wParam);
}
