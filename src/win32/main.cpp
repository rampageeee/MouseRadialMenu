#include "overlay.hpp"
#include <shellapi.h>
#include <array>
#include <chrono>
#include <cwchar>
#include <fstream>
#include <stdexcept>

namespace radial::win {
namespace {
constexpr UINT tray_message = WM_APP + 1, input_message = WM_APP + 2, focus_message = WM_APP + 3;
constexpr UINT_PTR active_timer = 1, dispatch_timer = 2, drain_timer = 3;
enum Command : WPARAM { Cancel = 1, Confirm = 2, Back = 3, HoldOpen = 4, HoldRelease = 5 };
enum TrayCommand { Enable = 100, Reload, Folder, Exit };
class App;
App* app = nullptr;

class App {
public:
    App(HINSTANCE instance, std::filesystem::path directory) : instance_(instance), directory_(std::move(directory)) {}
    ~App() {
        cancel(); remove_hooks(true);
        if (focus_hook_) UnhookWinEvent(focus_hook_);
        if (window_) {
            UnregisterHotKey(window_, hotkey_id_);
            Shell_NotifyIconW(NIM_DELETE, &tray_);
            DestroyWindow(window_);
        }
        app = nullptr;
    }
    void initialize() {
        app = this;
        auto candidate = load_config(directory_);
        WNDCLASSW wc{};
        wc.hInstance = instance_;
        wc.lpfnWndProc = window_proc;
        wc.lpszClassName = L"TinyRadialController";
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Cannot register controller");
        window_ = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"TinyRadial", 0, 0, 0, 0, 0, nullptr, nullptr, instance_, nullptr);
        if (!window_) throw std::runtime_error("Cannot create tray controller");
        overlay_.create(instance_);
        register_activation(*candidate, hotkey_id_);
        config_.commit(std::move(candidate));
        if (hold_enabled()) ensure_keyboard_hook();
        tray_.cbSize = sizeof(tray_);
        tray_.hWnd = window_;
        tray_.uID = 1;
        tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        tray_.uCallbackMessage = tray_message;
        tray_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wcscpy_s(tray_.szTip, L"TinyRadial - activation shortcut in config.json");
        if (!Shell_NotifyIconW(NIM_ADD, &tray_)) throw std::runtime_error("Cannot create tray icon");
        focus_hook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, foreground_proc,
            0, 0, WINEVENT_OUTOFCONTEXT);
        if (!focus_hook_) throw std::runtime_error("Cannot install foreground change observer");
        taskbar_message_ = RegisterWindowMessageW(L"TaskbarCreated");
    }
    LRESULT message(UINT message, WPARAM w, LPARAM l) {
        if (taskbar_message_ && message == taskbar_message_) { Shell_NotifyIconW(NIM_ADD, &tray_); return 0; }
        switch (message) {
        case WM_HOTKEY:
            if (static_cast<int>(w) != hotkey_id_ || !enabled_) return 0;
            // Hold owns primary down/up in the keyboard hook; registration reserves the chord.
            if (hold_enabled()) return 0;
            if (pending_) { cancel(); return 0; }
            if (menu_.active()) select(); else open();
            return 0;
        case WM_TIMER:
            if (w == active_timer) tick();
            else if (w == dispatch_timer) try_dispatch();
            else if (w == drain_timer) drain();
            return 0;
        case input_message:
            if (w == HoldOpen || w == HoldRelease) {
                hold_message(w, static_cast<std::uint32_t>(l));
                return 0;
            }
            if (static_cast<unsigned>(l) != session_) return 0;
            if (w == Cancel) cancel();
            else if (menu_.active() && w == Confirm) select();
            else if (menu_.active() && w == Back) {
                if (menu_.back()) overlay_.render(menu_, *snapshot_); else cancel();
            }
            return 0;
        case focus_message:
            // Compare the event's HWND too: switching away and back before this queue drains still cancels.
            if ((menu_.active() || pending_) && reinterpret_cast<HWND>(l) != target_.window) cancel();
            return 0;
        case WM_DISPLAYCHANGE: case WM_SETTINGCHANGE: case WM_DPICHANGED: case WM_POWERBROADCAST:
            cancel(); return message == WM_POWERBROADCAST ? TRUE : 0;
        case WM_QUERYENDSESSION: cancel(); return TRUE;
        case WM_ENDSESSION: if (w) PostQuitMessage(0); return 0;
        case tray_message:
            if (l == WM_RBUTTONUP || l == WM_CONTEXTMENU || l == WM_LBUTTONUP) tray_menu();
            return 0;
        case WM_COMMAND:
            switch (LOWORD(w)) {
            case Enable:
                cancel();
                if (enabled_) { UnregisterHotKey(window_, hotkey_id_); enabled_ = false; }
                else {
                    if (config_.current()->activation_behavior == ActivationBehavior::Hold) ensure_keyboard_hook();
                    try { register_activation(*config_.current(), hotkey_id_); }
                    catch (...) { drain(); throw; }
                    enabled_ = true;
                }
                drain();
                break;
            case Reload: cancel(); reload(); break;
            case Folder:
                cancel();
                if (reinterpret_cast<INT_PTR>(ShellExecuteW(window_, L"open", directory_.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
                    throw std::runtime_error("Cannot open configuration folder");
                break;
            case Exit: cancel(); PostQuitMessage(0); break;
            }
            return 0;
        case WM_CLOSE: cancel(); PostQuitMessage(0); return 0;
        }
        return DefWindowProcW(window_, message, w, l);
    }
    void error(const std::string& message) noexcept {
        cancel();
        try {
            auto path = directory_ / "errors.log";
            std::error_code ec;
            if (std::filesystem::file_size(path, ec) > 256 * 1024 && !ec) std::filesystem::remove(path, ec);
            std::ofstream log(path, std::ios::app);
            log << message << '\n';
            auto full = wide(message);
            // The modal text is never truncated and gives filenames and detailed diagnostics.
            MessageBoxW(window_, full.c_str(), L"TinyRadial", MB_OK | MB_ICONERROR);
        } catch (...) { MessageBoxW(window_, L"TinyRadial encountered an error and cancelled the menu.", L"TinyRadial", MB_OK | MB_ICONERROR); }
    }
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w, LPARAM l) noexcept {
        // During CreateWindowEx the App's HWND has not been assigned yet.
        if (!app || !app->window_) return DefWindowProcW(window, message, w, l);
        try { return app->message(message, w, l); }
        catch (const std::exception& e) { app->error(e.what()); return 0; }
        catch (...) { app->error("Unexpected Windows callback error"); return 0; }
    }
    static void CALLBACK foreground_proc(HWINEVENTHOOK, DWORD, HWND window, LONG, LONG, DWORD, DWORD) noexcept {
        if (app && app->hold_.valid() && app->hold_.holding() && window != app->hold_window_)
            app->hold_.cancel(); // Also invalidate a gesture whose opening is still queued.
        if (app && (app->menu_.active() || app->pending_)) PostMessageW(app->window_, focus_message, 0, reinterpret_cast<LPARAM>(window));
    }
    static LRESULT CALLBACK keyboard_proc(int code, WPARAM w, LPARAM l) noexcept {
        if (code == HC_ACTION && app) {
            const auto& event = *reinterpret_cast<KBDLLHOOKSTRUCT*>(l);
            if (!(event.flags & LLKHF_INJECTED) && event.vkCode < 256) {
                const auto key = static_cast<std::size_t>(event.vkCode);
                const bool up = w == WM_KEYUP || w == WM_SYSKEYUP;
                const bool repeat = app->physical_down_[key];
                app->physical_down_[key] = !up;
                auto config = app->config_.current();
                if (config) {
                    unsigned modifiers = 0;
                    if (!up && key == config->activation.key) {
                        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) modifiers |= Ctrl;
                        if (GetAsyncKeyState(VK_MENU) & 0x8000) modifiers |= Alt;
                        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) modifiers |= Shift;
                        if ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) modifiers |= Win;
                    }
                    auto action = app->hold_.event(config->activation, app->hold_enabled(),
                        static_cast<std::uint16_t>(key), up, repeat, modifiers);
                    if (action == HoldEvent::Open) {
                        app->hold_origin_valid_ = GetCursorPos(&app->hold_origin_) != FALSE;
                        app->hold_window_ = GetForegroundWindow();
                        app->hold_pid_ = 0;
                        if (app->hold_window_) GetWindowThreadProcessId(app->hold_window_, &app->hold_pid_);
                        app->post_hold(HoldOpen);
                    } else if (action == HoldEvent::Release) {
                        if (!app->hold_visible_ || app->menu_gesture_ != app->hold_.session()) app->hold_.cancel();
                        app->release_point_valid_ = GetCursorPos(&app->release_point_) != FALSE;
                        app->release_time_ = GetTickCount64();
                        app->post_hold(HoldRelease);
                    }
                    if (action != HoldEvent::Pass) return 1;
                }
                const bool control = key == VK_ESCAPE || key == VK_RETURN || key == VK_BACK;
                const bool opening_hold = app->hold_.holding() && app->hold_.valid();
                if (!up && key == VK_ESCAPE && opening_hold) app->hold_.cancel();
                if (app->preheld_[key]) {
                    if (up) app->preheld_[key] = false;
                } else if (up && app->swallowed_[key]) {
                    app->swallowed_[key] = false;
                    if (app->menu_.active() && key != VK_ESCAPE) app->post(key == VK_RETURN ? Confirm : Back);
                    return 1;
                } else if (!up && control && (app->menu_.active() || ((app->pending_ || opening_hold) && key == VK_ESCAPE))) {
                    const bool first = !app->swallowed_[key];
                    app->swallowed_[key] = true;
                    if (key == VK_ESCAPE && first) app->post(Cancel);
                    return 1;
                } else if (!up && app->swallowed_[key]) return 1;
            }
        }
        return CallNextHookEx(nullptr, code, w, l);
    }
    static LRESULT CALLBACK mouse_proc(int code, WPARAM w, LPARAM l) noexcept {
        if (code == HC_ACTION && app) {
            const auto& event = *reinterpret_cast<MSLLHOOKSTRUCT*>(l);
            if (!(event.flags & LLMHF_INJECTED)) {
                int button = -1;
                bool up = false;
                switch (w) {
                case WM_LBUTTONDOWN: button = 0; break;
                case WM_LBUTTONUP: button = 0; up = true; break;
                case WM_RBUTTONDOWN: button = 1; break;
                case WM_RBUTTONUP: button = 1; up = true; break;
                case WM_MBUTTONDOWN: button = 2; break;
                case WM_MBUTTONUP: button = 2; up = true; break;
                case WM_XBUTTONDOWN: button = HIWORD(event.mouseData) == XBUTTON1 ? 3 : 4; break;
                case WM_XBUTTONUP: button = HIWORD(event.mouseData) == XBUTTON1 ? 3 : 4; up = true; break;
                case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: if (app->menu_.active()) return 1; break;
                }
                if (button >= 0) {
                    const auto index = static_cast<std::size_t>(button);
                    if (up && app->mouse_down_[index]) {
                        app->mouse_down_[index] = false;
                        if (app->menu_.active()) app->post(button == 0 ? Confirm : button == 1 ? Back : Cancel);
                        return 1;
                    }
                    if (!up && app->menu_.active()) { app->mouse_down_[index] = true; return 1; }
                }
            }
        }
        return CallNextHookEx(nullptr, code, w, l);
    }
private:
    void post(Command command) noexcept { PostMessageW(window_, input_message, command, session_); }
    void post_hold(Command command) noexcept {
        if (!PostMessageW(window_, input_message, command, hold_.session())) hold_.cancel();
    }
    bool hold_enabled() const {
        auto config = config_.current();
        return enabled_ && config && config->activation_behavior == ActivationBehavior::Hold;
    }
    void hold_message(WPARAM command, std::uint32_t gesture) {
        if (gesture != hold_.session()) return; // Ignore queued events from earlier gestures.
        if (command == HoldOpen) {
            if (!hold_enabled()) { drain(); return; }
            if (menu_.active() || pending_ || draining()) { cancel(); return; }
            if (!hold_.valid() || !hold_.holding()) { drain(); return; }
            if (!hold_origin_valid_) { cancel(); throw std::runtime_error("Cannot capture activation pointer position"); }
            open(true);
        } else {
            if (menu_.active() && gesture == menu_gesture_ && snapshot_->activation_behavior == ActivationBehavior::Hold) {
                if (!hold_.valid() || !release_point_valid_ || !target_valid(target_)) { cancel(); return; }
                double x, y;
                overlay_.relative(release_point_, x, y);
                // Use release-time coordinates and time, not a later timer/cursor position.
                menu_.hover(x, y, release_time_);
                finish_selection(menu_.release());
            }
            drain();
        }
    }
    void register_activation(const Config& config, int id) {
        if (!RegisterHotKey(window_, id, config.activation.modifiers | MOD_NOREPEAT, config.activation.key))
            throw std::runtime_error("Cannot register activation '" + config.activation.text + "'. It may be reserved or used by another app. Edit config.json and reload.");
    }
    void reload() {
        auto candidate = load_config(directory_);
        try {
            if (enabled_ && candidate->activation_behavior == ActivationBehavior::Hold) ensure_keyboard_hook();
            if (enabled_ && !(candidate->activation == config_.current()->activation)) {
                int new_id = hotkey_id_ == 1 ? 2 : 1;
                // Reserve the candidate before unregistering the last-known-good hotkey.
                register_activation(*candidate, new_id);
                UnregisterHotKey(window_, hotkey_id_);
                hotkey_id_ = new_id;
            }
        } catch (...) { drain(); throw; }
        config_.commit(std::move(candidate));
        drain();
    }
    void ensure_keyboard_hook() {
        if (keyboard_hook_) return;
        // A trigger already held when enabling/reloading must not open on autorepeat.
        for (std::size_t key = 0; key < physical_down_.size(); ++key)
            physical_down_[key] = (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
        keyboard_hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_proc, instance_, 0);
        if (!keyboard_hook_) throw std::runtime_error("Cannot install keyboard controls; activation cancelled");
    }
    void install_hooks() {
        swallowed_.fill(false); mouse_down_.fill(false); preheld_.fill(false);
        for (int key : {VK_ESCAPE, VK_RETURN, VK_BACK}) preheld_[static_cast<std::size_t>(key)] = (GetAsyncKeyState(key) & 0x8000) != 0;
        ensure_keyboard_hook();
        mouse_hook_ = SetWindowsHookExW(WH_MOUSE_LL, mouse_proc, instance_, 0);
        if (!keyboard_hook_ || !mouse_hook_) { remove_hooks(); throw std::runtime_error("Cannot install menu controls; opening cancelled"); }
    }
    void remove_hooks(bool force = false) noexcept {
        // Hold needs a lightweight observer while enabled, or until its owned key-up.
        if (keyboard_hook_ && (force || (!hold_enabled() && !hold_.holding()))) {
            UnhookWindowsHookEx(keyboard_hook_);
            keyboard_hook_ = nullptr;
        }
        if (mouse_hook_) UnhookWindowsHookEx(mouse_hook_);
        mouse_hook_ = nullptr;
        swallowed_.fill(false); mouse_down_.fill(false);
        if (window_) KillTimer(window_, drain_timer);
    }
    bool draining() const noexcept {
        for (bool down : swallowed_) if (down) return true;
        for (bool down : mouse_down_) if (down) return true;
        return false;
    }
    void drain() noexcept {
        if (menu_.active()) return;
        // Only suppress the matching ups of control downs we consumed. All other input passes.
        if (!draining() || GetTickCount64() >= drain_until_) {
            if (pending_) {
                // Keep the Esc hook until deferred dispatch completes; polling can miss a short tap.
                if (mouse_hook_) UnhookWindowsHookEx(mouse_hook_);
                mouse_hook_ = nullptr;
                KillTimer(window_, drain_timer);
            } else remove_hooks();
        }
    }
    void open(bool from_hold = false) {
        if (draining()) return;
        remove_hooks();
        snapshot_ = config_.current();
        target_ = capture_target();
        if (from_hold) {
            if (target_.window != hold_window_ || target_.pid != hold_pid_) { cancel(); return; }
            origin_ = hold_origin_;
            menu_gesture_ = hold_.session();
        } else if (!GetCursorPos(&origin_)) throw std::runtime_error("Cannot capture pointer position");
        restore_ = true;
        overlay_.place(origin_);
        const auto& profile = snapshot_->match(target_.executable);
        menu_.open(profile.menu, snapshot_->dead_zone, snapshot_->dwell_ms);
        ++session_;
        install_hooks();
        if (!SetTimer(window_, active_timer, 16, nullptr)) throw std::runtime_error("Cannot start menu timer");
        overlay_.render(menu_, *snapshot_);
        hold_visible_ = from_hold && hold_.valid() && hold_.holding();
    }
    void tick() {
        if (!menu_.active()) return;
        if (!target_valid(target_)) { cancel(); return; }
        if (snapshot_->activation_behavior == ActivationBehavior::Hold) {
            if (!hold_.valid() || hold_.session() != menu_gesture_) { cancel(); return; }
            if (!hold_.holding()) return; // A queued release owns the final hit test.
        }
        POINT point{};
        if (!GetCursorPos(&point)) { cancel(); return; }
        double x, y;
        overlay_.relative(point, x, y);
        if (menu_.hover(x, y, GetTickCount64())) overlay_.render(menu_, *snapshot_);
    }
    void select() {
        if (menu_.active() && snapshot_->activation_behavior == ActivationBehavior::Hold && !hold_.holding()) return;
        const Menu* before = menu_.active() ? &menu_.menu() : nullptr;
        tick();
        if (!menu_.active()) return;
        // A dwell transition in tick is already the confirmation gesture for this submenu.
        if (&menu_.menu() != before) return;
        if (snapshot_->activation_behavior == ActivationBehavior::Hold && menu_.selected() >= 0 &&
            !menu_.menu().items[static_cast<std::size_t>(menu_.selected())].submenu) return;
        auto chord = menu_.confirm();
        if (menu_.active()) { overlay_.render(menu_, *snapshot_); return; }
        finish_selection(std::move(chord));
    }
    void finish_selection(std::optional<Chord> chord) {
        hold_.cancel();
        pending_ = std::move(chord);
        close_menu();
        if (pending_) {
            dispatch_until_ = GetTickCount64() + 2000;
            if (!SetTimer(window_, dispatch_timer, 10, nullptr)) { pending_.reset(); throw std::runtime_error("Cannot schedule shortcut dispatch"); }
        }
    }
    void close_menu() noexcept {
        hold_visible_ = false;
        menu_.close(); overlay_.hide();
        if (window_) KillTimer(window_, active_timer);
        if (restore_) { SetCursorPos(origin_.x, origin_.y); restore_ = false; }
        if (draining() || pending_) {
            drain_until_ = GetTickCount64() + 2000;
            if (!SetTimer(window_, drain_timer, 25, nullptr)) remove_hooks();
        } else remove_hooks();
    }
    void cancel() noexcept {
        hold_.cancel();
        pending_.reset();
        if (window_) KillTimer(window_, dispatch_timer);
        close_menu();
    }
    void try_dispatch() {
        if (!pending_) { KillTimer(window_, dispatch_timer); return; }
        if (snapshot_->activation_behavior == ActivationBehavior::Hold && hold_.session() != menu_gesture_) { cancel(); return; }
        if (!target_valid(target_) || GetTickCount64() >= dispatch_until_ || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { cancel(); return; }
        if (draining() || !keys_released(*pending_, snapshot_->activation)) return;
        remove_hooks();
        auto chord = *pending_;
        pending_.reset(); KillTimer(window_, dispatch_timer);
        dispatch(target_, chord);
    }
    void tray_menu() {
        cancel();
        HMENU menu = CreatePopupMenu();
        if (!menu) throw std::runtime_error("Cannot create tray menu");
        AppendMenuW(menu, MF_STRING | (enabled_ ? MF_CHECKED : MF_UNCHECKED), Enable, L"Enabled");
        AppendMenuW(menu, MF_STRING, Reload, L"Reload configuration");
        AppendMenuW(menu, MF_STRING, Folder, L"Open configuration folder");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, Exit, L"Exit");
        POINT point{}; GetCursorPos(&point);
        SetForegroundWindow(window_);
        UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, window_, nullptr);
        DestroyMenu(menu);
        PostMessageW(window_, WM_NULL, 0, 0);
        if (command) message(WM_COMMAND, command, 0);
    }
    HINSTANCE instance_;
    std::filesystem::path directory_;
    HWND window_{};
    NOTIFYICONDATAW tray_{};
    UINT taskbar_message_{};
    ConfigStore config_;
    std::shared_ptr<const Config> snapshot_;
    Overlay overlay_;
    MenuState menu_;
    Target target_;
    POINT origin_{};
    bool restore_ = false, enabled_ = true;
    int hotkey_id_ = 1;
    HHOOK keyboard_hook_{}, mouse_hook_{};
    HWINEVENTHOOK focus_hook_{};
    std::array<bool, 256> swallowed_{}, preheld_{};
    std::array<bool, 256> physical_down_{};
    std::array<bool, 5> mouse_down_{};
    HoldActivation hold_;
    std::uint32_t menu_gesture_ = 0;
    POINT hold_origin_{}, release_point_{};
    HWND hold_window_{};
    DWORD hold_pid_{};
    bool hold_origin_valid_ = false, release_point_valid_ = false, hold_visible_ = false;
    ULONGLONG release_time_{};
    std::optional<Chord> pending_;
    ULONGLONG dispatch_until_{}, drain_until_{};
    unsigned session_ = 0;
};
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    using namespace radial::win;
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        MessageBoxW(nullptr, L"Per-monitor DPI awareness could not be enabled.", L"TinyRadial", MB_OK | MB_ICONERROR);
        return 1;
    }
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\TinyRadial-0.1");
    if (!mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"TinyRadial is already running. Use its tray icon.", L"TinyRadial", MB_OK);
        CloseHandle(mutex); return 0;
    }
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com)) { CloseHandle(mutex); return 1; }
    int result = 0;
    try {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv) throw std::runtime_error("Cannot read command line");
        auto directory = executable_directory();
        bool valid = argc == 1 || (argc == 3 && std::wstring(argv[1]) == L"--config-dir");
        if (argc == 3 && valid) directory = argv[2];
        LocalFree(argv);
        if (!valid) throw std::runtime_error("Usage: TinyRadial.exe [--config-dir <folder>]");
        App application(instance, directory);
        application.initialize();
        MSG message{};
        BOOL status;
        while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
        result = status < 0 ? 1 : static_cast<int>(message.wParam);
    } catch (const std::exception& e) {
        try { MessageBoxW(nullptr, wide(e.what()).c_str(), L"TinyRadial startup", MB_OK | MB_ICONERROR); }
        catch (...) { MessageBoxW(nullptr, L"Startup failed. Check the configuration and folder permissions.", L"TinyRadial startup", MB_OK | MB_ICONERROR); }
        result = 1;
    }
    CoUninitialize(); CloseHandle(mutex);
    return result;
}
