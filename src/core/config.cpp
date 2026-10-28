#include "core.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>

namespace radial {
namespace {
using Json = nlohmann::json;
std::string filename(const std::filesystem::path& path) {
    auto value = path.u8string();
    return {value.begin(), value.end()};
}
[[noreturn]] void invalid(const std::string& where, const std::string& why) {
    throw std::runtime_error(where + ": " + why);
}
void fields(const Json& value, std::initializer_list<std::string_view> allowed, const std::string& where) {
    if (!value.is_object()) invalid(where, "expected an object");
    for (const auto& [key, unused] : value.items()) {
        (void)unused;
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) invalid(where, "unknown field '" + key + "'");
    }
}
const Json& required(const Json& obj, const char* key, const std::string& where) {
    if (!obj.contains(key)) invalid(where, "missing '" + std::string(key) + "'");
    return obj.at(key);
}
std::string text(const Json& value, const std::string& where, std::size_t max = 120) {
    if (!value.is_string()) invalid(where, "expected a string");
    auto s = value.get<std::string>();
    if (s.empty() || s.size() > max || std::all_of(s.begin(), s.end(), [](unsigned char c){return c <= 32;}))
        invalid(where, "must be nonempty and at most " + std::to_string(max) + " UTF-8 bytes");
    for (unsigned char c : s) if (c < 32 || c == 127) invalid(where, "control characters are not allowed");
    return s;
}
int integer(const Json& value, int low, int high, const std::string& where) {
    if (!value.is_number_integer() || value < low || value > high)
        invalid(where, "expected an integer from " + std::to_string(low) + " to " + std::to_string(high));
    return value.get<int>();
}
void version(const Json& value, const std::string& where) {
    integer(required(value, "schemaVersion", where), 1, 1, where + ".schemaVersion");
}
Json read(const std::filesystem::path& file) {
    try {
        auto size = std::filesystem::file_size(file);
        if (size > 1024 * 1024) invalid(filename(file), "file exceeds 1 MiB limit");
        std::ifstream in(file, std::ios::binary);
        if (!in) invalid(filename(file), "cannot open file");
        std::vector<std::set<std::string>> keys;
        std::size_t nodes = 0;
        // Bound the JSON parser itself, before building the validated menu tree.
        auto guard = [&](int depth, Json::parse_event_t event, Json& parsed) {
            if (depth > 80 || ++nodes > 50000) throw std::runtime_error("JSON nesting/node limit exceeded");
            if (event == Json::parse_event_t::object_start) keys.emplace_back();
            else if (event == Json::parse_event_t::object_end) keys.pop_back();
            else if (event == Json::parse_event_t::key && !keys.back().insert(parsed.get<std::string>()).second)
                throw std::runtime_error("duplicate JSON field '" + parsed.get<std::string>() + "'");
            return true;
        };
        return Json::parse(in, guard);
    } catch (const std::exception& e) { invalid(filename(file), e.what()); }
}
std::shared_ptr<const Menu> menu(const Json& value, const std::string& where, int depth, const Chord& activation) {
    if (depth > 16) invalid(where, "submenu nesting exceeds 16 levels");
    fields(value, {"label", "items"}, where);
    auto result = std::make_shared<Menu>();
    result->label = text(required(value, "label", where), where + ".label", 80);
    const auto& items = required(value, "items", where);
    if (!items.is_array() || items.empty() || items.size() > 8) invalid(where + ".items", "expected 1 to 8 items");
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto item_path = where + ".items[" + std::to_string(i) + "]";
        const auto& v = items[i];
        fields(v, {"label", "keys", "submenu"}, item_path);
        Item item;
        item.label = text(required(v, "label", item_path), item_path + ".label", 80);
        if (v.contains("keys") == v.contains("submenu")) invalid(item_path, "provide exactly one of keys or submenu");
        if (v.contains("keys")) {
            try {
                item.chord = parse_chord(text(v.at("keys"), item_path + ".keys"));
                if (*item.chord == activation) invalid(item_path, "action cannot equal the activation hotkey");
            }
            catch (const std::exception& e) { invalid(item_path + ".keys", e.what()); }
        } else item.submenu = menu(v.at("submenu"), item_path + ".submenu", depth + 1, activation);
        result->items.push_back(std::move(item));
    }
    return result;
}
std::filesystem::path profile_path(const std::filesystem::path& root, const std::string& name) {
    const std::filesystem::path path(std::u8string(name.begin(), name.end()));
    if (path.is_absolute() || path.has_root_name() || name.find(':') != std::string::npos || name.find('\\') != std::string::npos)
        invalid(name, "profile paths must be relative and use forward slashes");
    for (const auto& part : path) if (part == "..") invalid(name, "parent traversal is not allowed");
    auto canonical = std::filesystem::canonical(root / path);
    auto relative = canonical.lexically_relative(root);
    if (relative.empty() || *relative.begin() == "..") invalid(name, "profile must remain inside config folder");
    return canonical;
}
}
const Profile& Config::match(std::string executable) const {
    executable = ascii_lower(std::move(executable));
    const Profile* fallback = nullptr;
    for (const auto& profile : profiles) {
        if (profile.executables.empty()) fallback = &profile;
        for (const auto& name : profile.executables) if (name == executable) return profile;
    }
    if (!fallback) throw std::logic_error("validated configuration has no default profile");
    return *fallback;
}
std::shared_ptr<const Config> load_config(const std::filesystem::path& directory) {
    const auto file = directory / "config.json";
    try {
        auto root = std::filesystem::canonical(directory);
        auto json = read(file);
        const auto where = filename(file);
        fields(json, {"schemaVersion", "activation", "ui", "profiles"}, where);
        version(json, where);
        auto config = std::make_shared<Config>();
        const auto& activation = required(json, "activation", where);
        fields(activation, {"hotkey", "behavior", "cursorMode"}, where + ".activation");
        config->activation = parse_chord(text(required(activation, "hotkey", where), where + ".activation.hotkey"));
        if (config->activation.key == 0x1B || config->activation.key == 0x0D || config->activation.key == 0x08 || config->activation.key == 0x7B)
            invalid(where + ".activation.hotkey", "Esc/Enter/Backspace are menu controls; F12 is reserved for debugging");
        auto behavior = text(required(activation, "behavior", where), where + ".activation.behavior");
        if (behavior == "hold") config->activation_behavior = ActivationBehavior::Hold;
        else if (behavior != "toggle") invalid(where + ".activation.behavior", "expected 'toggle' or 'hold'");
        if (text(required(activation, "cursorMode", where), where) != "restore")
            invalid(where + ".activation.cursorMode", "only 'restore' is supported; virtual cursor is deferred");
        const auto& ui = required(json, "ui", where);
        fields(ui, {"deadZonePx", "submenuDwellMs", "showShortcuts"}, where + ".ui");
        config->dead_zone = integer(required(ui, "deadZonePx", where), 16, 60, where + ".ui.deadZonePx");
        config->dwell_ms = static_cast<std::uint64_t>(integer(required(ui, "submenuDwellMs", where), 150, 2000, where + ".ui.submenuDwellMs"));
        const auto& show = required(ui, "showShortcuts", where);
        if (!show.is_boolean()) invalid(where + ".ui.showShortcuts", "expected true or false");
        config->show_shortcuts = show.get<bool>();
        const auto& paths = required(json, "profiles", where);
        if (!paths.is_array() || paths.empty() || paths.size() > 64) invalid(where + ".profiles", "expected 1 to 64 profile paths");
        std::set<std::string> executables;
        int fallbacks = 0;
        for (const auto& path : paths) {
            const auto profile_file = profile_path(root, text(path, where + ".profiles", 240));
            const auto pw = filename(profile_file);
            auto p = read(profile_file);
            fields(p, {"schemaVersion", "name", "match", "menu"}, pw);
            version(p, pw);
            Profile profile;
            profile.name = text(required(p, "name", pw), pw + ".name");
            const auto& match = required(p, "match", pw);
            fields(match, {"executables"}, pw + ".match");
            const auto& names = required(match, "executables", pw);
            if (!names.is_array() || names.size() > 64) invalid(pw + ".match.executables", "expected an array of at most 64 filenames");
            for (const auto& name : names) {
                auto exe = ascii_lower(text(name, pw + ".match.executables", 255));
                if (exe.find_first_of("/\\:*?\"") != std::string::npos || !exe.ends_with(".exe"))
                    invalid(pw, "match must be an executable filename ending in .exe");
                if (!executables.insert(exe).second) invalid(pw, "duplicate executable match '" + exe + "'; each app may match only once");
                profile.executables.push_back(std::move(exe));
            }
            if (profile.executables.empty()) ++fallbacks;
            profile.menu = menu(required(p, "menu", pw), pw + ".menu", 1, config->activation);
            config->profiles.push_back(std::move(profile));
        }
        if (fallbacks != 1) invalid(where, "exactly one global profile with empty executables is required");
        return config;
    } catch (const std::exception& e) { invalid(filename(file), e.what()); }
}
}
