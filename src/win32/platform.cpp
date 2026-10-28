#include "platform.hpp"
#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace radial::win {
namespace {
class Handle {
public:
    explicit Handle(HANDLE h) : h_(h) {}
    ~Handle() { if (h_ && h_ != INVALID_HANDLE_VALUE) CloseHandle(h_); }
    HANDLE get() const { return h_; }
private:
    HANDLE h_;
};
DWORD integrity(HANDLE process) {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &raw)) throw std::runtime_error("Cannot query target privilege level");
    Handle token(raw);
    DWORD size = 0;
    GetTokenInformation(token.get(), TokenIntegrityLevel, nullptr, 0, &size);
    if (!size) throw std::runtime_error("Cannot query privilege information size");
    std::vector<unsigned char> buffer(size);
    if (!GetTokenInformation(token.get(), TokenIntegrityLevel, buffer.data(), size, &size))
        throw std::runtime_error("Cannot read target privilege level");
    auto label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer.data());
    auto sid = label->Label.Sid;
    return *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1);
}
bool extended(std::uint16_t key) {
    return (key >= VK_PRIOR && key <= VK_DOWN) || key == VK_INSERT || key == VK_DELETE || key == VK_LWIN;
}
INPUT event(std::uint16_t key, bool up) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key;
    input.ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0) | (extended(key) ? KEYEVENTF_EXTENDEDKEY : 0);
    input.ki.dwExtraInfo = static_cast<ULONG_PTR>(0x54494E59); // TINY, for the diagnostic probe.
    return input;
}
}
std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!n) throw std::runtime_error("Invalid UTF-8 text");
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}
std::string utf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (!n) throw std::runtime_error("Cannot convert Windows path to UTF-8");
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}
Target capture_target() {
    Target target;
    target.window = GetForegroundWindow();
    if (!target.window || IsIconic(target.window)) throw std::runtime_error("No usable foreground window");
    GetWindowThreadProcessId(target.window, &target.pid);
    if (!target.pid || target.pid == GetCurrentProcessId()) throw std::runtime_error("Select an application before opening the menu");
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, target.pid));
    if (!process.get()) throw std::runtime_error("Cannot inspect foreground application");
    wchar_t path[32768];
    DWORD length = static_cast<DWORD>(std::size(path));
    if (!QueryFullProcessImageNameW(process.get(), 0, path, &length)) throw std::runtime_error("Cannot read foreground executable");
    target.executable = utf8(std::filesystem::path(std::wstring(path, length)).filename().wstring());
    if (integrity(process.get()) > integrity(GetCurrentProcess()))
        throw std::runtime_error("Target is elevated. TinyRadial refuses to send shortcuts to a higher-privilege application.");
    if (!target_valid(target)) throw std::runtime_error("Foreground changed while capturing application");
    return target;
}
bool target_valid(const Target& target) {
    DWORD pid = 0;
    if (!target.window || !IsWindow(target.window) || IsIconic(target.window) || !IsWindowVisible(target.window) || GetForegroundWindow() != target.window) return false;
    GetWindowThreadProcessId(target.window, &pid);
    return target.pid == pid;
}
bool keys_released(const Chord& action, const Chord& activation) {
    for (int key : {VK_CONTROL, VK_MENU, VK_SHIFT, VK_LWIN, VK_RWIN})
        if (GetAsyncKeyState(key) & 0x8000) return false;
    return !(GetAsyncKeyState(action.key) & 0x8000) && !(GetAsyncKeyState(activation.key) & 0x8000);
}
void dispatch(const Target& target, const Chord& chord) {
    if (!target_valid(target)) throw std::runtime_error("Shortcut cancelled: original target is no longer foreground");
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, target.pid));
    if (!process.get() || integrity(process.get()) > integrity(GetCurrentProcess()))
        throw std::runtime_error("Shortcut cancelled: target privileges could not be verified");
    auto keys = chord_keys(chord);
    std::vector<INPUT> events;
    for (auto key : keys) events.push_back(event(key, false));
    for (auto i = keys.rbegin(); i != keys.rend(); ++i) events.push_back(event(*i, true));
    // Send a complete balanced chord in one serial, non-interleaved OS batch.
    if (!target_valid(target)) return;
    UINT sent = SendInput(static_cast<UINT>(events.size()), events.data(), sizeof(INPUT));
    if (sent == events.size()) return;
    // Release only keys whose key-down was accepted, excluding accepted key-ups.
    std::vector<std::uint16_t> held;
    for (UINT i = 0; i < sent; ++i) {
        if (!(events[i].ki.dwFlags & KEYEVENTF_KEYUP)) held.push_back(events[i].ki.wVk);
        else held.erase(std::remove(held.begin(), held.end(), events[i].ki.wVk), held.end());
    }
    std::vector<INPUT> cleanup;
    for (auto i = held.rbegin(); i != held.rend(); ++i) cleanup.push_back(event(*i, true));
    UINT released = 0;
    // Two bounded attempts, retrying only the remaining releases, never key-downs.
    for (int attempt = 0; attempt < 2 && released < cleanup.size(); ++attempt)
        released += SendInput(static_cast<UINT>(cleanup.size()) - released, cleanup.data() + released, sizeof(INPUT));
    if (released < cleanup.size())
        throw std::runtime_error("Windows rejected the shortcut AND some cleanup key-ups. Release Ctrl/Alt/Shift/Win physically, then disable TinyRadial. Check target privileges.");
    throw std::runtime_error("Windows rejected part of the shortcut; cleanup key-ups were submitted. Check target privileges.");
}
std::filesystem::path executable_directory() {
    wchar_t buffer[32768];
    DWORD size = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (!size || size == std::size(buffer)) throw std::runtime_error("Cannot locate executable folder");
    return std::filesystem::path(std::wstring(buffer, size)).parent_path();
}
}
