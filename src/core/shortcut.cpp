#include "core.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace radial {
std::string ascii_lower(std::string value) {
    for (auto& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
}
Chord parse_chord(std::string_view text) {
    auto fail = [&]() -> void { throw std::runtime_error("invalid single chord '" + std::string(text) +
        "' (use Ctrl/Alt/Shift/Win plus one named key; sequences and punctuation keys are unsupported)"); };
    if (text.empty() || text.size() > 100) fail();
    Chord result{0, 0, std::string(text)};
    std::size_t start = 0;
    while (start <= text.size()) {
        auto end = text.find('+', start);
        if (end == std::string_view::npos) end = text.size();
        auto token = ascii_lower(std::string(text.substr(start, end - start)));
        unsigned mod = token == "ctrl" ? unsigned(Ctrl) : token == "alt" ? unsigned(Alt) : token == "shift" ? unsigned(Shift) : token == "win" ? unsigned(Win) : 0U;
        if (mod) {
            if ((result.modifiers & mod) || result.key) fail();
            result.modifiers |= mod;
        } else {
            if (result.key || token.empty()) fail();
            if (token.size() == 1 && ((token[0] >= 'a' && token[0] <= 'z') || (token[0] >= '0' && token[0] <= '9')))
                result.key = static_cast<std::uint16_t>(token[0] >= 'a' ? token[0] - 'a' + 'A' : token[0]);
            else if (token[0] == 'f' && token.size() <= 3 && token.size() >= 2) {
                int n = 0;
                for (std::size_t i = 1; i < token.size(); ++i) {
                    if (token[i] < '0' || token[i] > '9') fail();
                    n = n * 10 + token[i] - '0';
                }
                if (n < 1 || n > 24 || token[1] == '0') fail();
                result.key = static_cast<std::uint16_t>(0x70 + n - 1);
            } else {
                constexpr std::pair<std::string_view, std::uint16_t> names[] = {
                    {"space",0x20},{"enter",0x0D},{"tab",0x09},{"esc",0x1B},
                    {"backspace",0x08},{"delete",0x2E},{"insert",0x2D},
                    {"home",0x24},{"end",0x23},{"pageup",0x21},{"pagedown",0x22},
                    {"left",0x25},{"up",0x26},{"right",0x27},{"down",0x28}};
                for (const auto& [name, key] : names) if (token == name) result.key = key;
                if (!result.key) fail();
            }
        }
        if (end == text.size()) break;
        start = end + 1;
    }
    if (!result.key) fail();
    return result;
}
std::vector<std::uint16_t> chord_keys(const Chord& chord) {
    std::vector<std::uint16_t> keys;
    if (chord.modifiers & Ctrl) keys.push_back(0x11);
    if (chord.modifiers & Alt) keys.push_back(0x12);
    if (chord.modifiers & Shift) keys.push_back(0x10);
    if (chord.modifiers & Win) keys.push_back(0x5B);
    keys.push_back(chord.key);
    return keys;
}
}
