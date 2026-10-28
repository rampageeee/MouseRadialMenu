#include "core/core.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace radial;
using Json = nlohmann::json;
namespace {
int checks = 0;
void check(bool ok, const std::string& message) {
    ++checks;
    if (!ok) throw std::runtime_error(message);
}
template<class F> void rejects(F operation, const std::string& expected) {
    try { operation(); }
    catch (const std::exception& e) { check(std::string(e.what()).find(expected) != std::string::npos, std::string("wrong diagnostic: ") + e.what()); return; }
    throw std::runtime_error("expected rejection: " + expected);
}
struct Fixture {
    std::filesystem::path directory = std::filesystem::temp_directory_path() /
        ("tinyradial-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Json config, profile;
    Fixture() {
        directory += std::filesystem::path(u8"-é-配置");
        std::filesystem::create_directories(directory / "profiles");
        std::ifstream c(std::filesystem::path(SAMPLE_ROOT) / "config.json"); c >> config;
        std::ifstream p(std::filesystem::path(SAMPLE_ROOT) / "profiles/default.json"); p >> profile;
        config["profiles"] = {"profiles/default.json"};
        save();
    }
    ~Fixture() { std::error_code ec; std::filesystem::remove_all(directory, ec); }
    void save() {
        std::ofstream(directory / "config.json") << config.dump(2);
        std::ofstream(directory / "profiles/default.json") << profile.dump(2);
    }
    void bad_config(const std::function<void(Json&)>& edit, const std::string& expected) {
        auto old = config; edit(config); save();
        rejects([&]{load_config(directory);}, expected);
        config = old; save();
    }
    void bad_profile(const std::function<void(Json&)>& edit, const std::string& expected) {
        auto old = profile; edit(profile); save();
        rejects([&]{load_config(directory);}, expected);
        profile = old; save();
    }
};
void shortcuts() {
    check(parse_chord("Ctrl+Shift+S").key == 'S', "letter chord");
    check(parse_chord("alt+Enter").modifiers == Alt, "case insensitive modifiers");
    check(parse_chord("Win+0").key == '0', "digit chord");
    check(parse_chord("Ctrl+Alt+Shift+Win+Delete").modifiers == 15, "all modifiers");
    for (int n = 1; n <= 24; ++n) check(parse_chord("F" + std::to_string(n)).key == 0x70 + n - 1, "function key mapping");
    for (const auto* name : {"Space","Enter","Tab","Esc","Backspace","Delete","Insert","Home","End","PageUp","PageDown","Left","Up","Right","Down"})
        check(parse_chord(name).key != 0, "navigation key");
    for (const auto* bad : {"", "Ctrl", "Control+A", "Ctrl++", "+A", "A+", "Ctrl+Ctrl+A", "A+Ctrl", "A+B", "Ctrl+K, Ctrl+C", "F0", "F25", "F01", "F1x", "Ctrl+ A", "Ctrl+;", "Ctrl+Plus", "SHIFT+Shift+A", "Ctrl+Alt"})
        rejects([&]{parse_chord(bad);}, "invalid single chord");
    check(chord_keys(parse_chord("Win+Shift+Alt+Ctrl+S")) == std::vector<std::uint16_t>({0x11,0x12,0x10,0x5b,'S'}), "deterministic modifier down order");
}
void configuration() {
    auto samples = load_config(SAMPLE_ROOT);
    check(samples->match("ReApEr.ExE").name == "REAPER", "case insensitive app match");
    check(samples->match("blender.exe").name == "Blender", "app match");
    check(samples->match("unknown.exe").name == "Desktop", "fallback");
    check(samples->match("").name == "Desktop", "empty app fallback");
    Fixture f;
    auto good = load_config(f.directory);
    ConfigStore store; store.commit(good);
    auto snapshot = store.current();
    f.bad_config([](Json& v){v["schemaVersion"] = 2;}, "schemaVersion");
    f.bad_config([](Json& v){v["ui"]["deadZonePx"] = 0;}, "deadZonePx");
    f.bad_config([](Json& v){v["ui"]["deadZonePx"] = 28.5;}, "integer");
    f.bad_config([](Json& v){v["ui"]["submenuDwellMs"] = 999999999999ULL;}, "submenuDwellMs");
    f.bad_config([](Json& v){v["ui"]["showShortcuts"] = "yes";}, "true or false");
    f.bad_config([](Json& v){v["activation"]["behavior"] = "hold";}, "toggle");
    f.bad_config([](Json& v){v["activation"]["cursorMode"] = "virtual";}, "restore");
    for (const auto* key : {"Esc", "Ctrl+Enter", "Backspace", "F12"})
        f.bad_config([&](Json& v){v["activation"]["hotkey"] = key;}, "menu controls");
    f.bad_config([](Json& v){v["surprise"] = 1;}, "unknown field");
    f.bad_config([](Json& v){v.erase("activation");}, "missing 'activation'");
    f.bad_config([](Json& v){v["profiles"] = Json::array();}, "1 to 64");
    f.bad_config([](Json& v){v["profiles"] = {"../escape.json"};}, "parent traversal");
    f.bad_config([](Json& v){v["profiles"] = {"C:\\outside.json"};}, "relative");
    f.bad_config([](Json& v){v["profiles"] = {"missing.json"};}, "missing.json");
    f.bad_config([](Json& v){v["profiles"].push_back("profiles/default.json");}, "exactly one global");
    f.bad_profile([](Json& v){v["match"]["executables"] = {"reaper.exe"};}, "global profile");
    f.bad_profile([](Json& v){v["match"]["executables"] = {"reaper.exe", "REAPER.EXE"};}, "duplicate executable");
    f.bad_profile([](Json& v){v["match"]["executables"] = {"folder/reaper.exe"};}, "executable filename");
    f.bad_profile([](Json& v){v["menu"]["items"] = Json::array();}, "1 to 8");
    f.bad_profile([](Json& v){auto item=v["menu"]["items"][0]; v["menu"]["items"]=Json::array(); for(int i=0;i<9;++i)v["menu"]["items"].push_back(item);}, "1 to 8");
    f.bad_profile([](Json& v){v["menu"]["items"][0]["label"] = "";}, "nonempty");
    f.bad_profile([](Json& v){v["menu"]["items"][0]["label"] = "   ";}, "nonempty");
    f.bad_profile([](Json& v){v["menu"]["items"][0]["label"] = "a\nb";}, "control characters");
    f.bad_profile([](Json& v){v["menu"]["items"][0]["submenu"] = v["menu"];}, "exactly one");
    f.bad_profile([](Json& v){v["menu"]["items"][0].erase("keys");}, "exactly one");
    f.bad_profile([](Json& v){v["menu"]["items"][0]["keys"] = "Ctrl+K,Ctrl+C";}, "items[0].keys");
    f.bad_profile([](Json& v){v["menu"]["items"][0]["keys"] = "Ctrl+Alt+Space";}, "cannot equal the activation");
    f.bad_profile([](Json& v){
        Json m = {{"label","Deep"},{"items",Json::array({{{"label","Leaf"},{"keys","F13"}}})}};
        for(int i=0;i<17;++i) m={{"label","Nested"},{"items",Json::array({{{"label","Next"},{"submenu",m}}})}};
        v["menu"] = m;
    }, "nesting exceeds 16");
    std::ofstream(f.directory / "profiles/default.json") << "{\"schemaVersion\":1,\"schemaVersion\":1}";
    rejects([&]{load_config(f.directory);}, "duplicate JSON field");
    f.save();
    std::ofstream(f.directory / "config.json") << "{ bad json";
    rejects([&]{store.commit(load_config(f.directory));}, "config.json");
    check(store.current() == good, "invalid reload preserves last-known-good");
    f.config["ui"]["deadZonePx"] = 40; f.save();
    store.commit(load_config(f.directory));
    check(store.current()->dead_zone == 40 && snapshot->dead_zone == 28, "atomic reload keeps open snapshot valid");
    std::ofstream(f.directory / "config.json") << std::string(1024*1024 + 1, ' ');
    rejects([&]{load_config(f.directory);}, "1 MiB");
    std::ofstream(f.directory / "config.json") << std::string(90, '[') << "0" << std::string(90, ']');
    rejects([&]{load_config(f.directory);}, "nesting/node limit");
}
void navigation() {
    auto config = load_config(SAMPLE_ROOT);
    auto root = config->match("anything.exe").menu;
    MenuState m;
    m.open(root, 28, 220);
    check(m.active() && m.selected() == -1, "open unselected");
    m.hover(0,0,0); check(!m.confirm() && !m.active(), "center cancel");
    m.open(root,28,220); m.hover(0,-120,100);
    auto result=m.confirm(); check(result && result->text == "Ctrl+C" && !m.active(), "leaf dispatch once");
    check(!m.confirm(), "closed state cannot dispatch twice");
    m.open(root,28,220); m.hover(0,120,100);
    check(m.selected()==2 && !m.nested(), "submenu hover");
    m.hover(0,120,319); check(!m.nested(), "dwell threshold not early");
    m.hover(0,120,320); check(m.nested() && m.selected()==-1, "dwell opens submenu");
    m.hover(0,-120,600); check(m.selected()==-1, "submenu needs center rearm");
    m.hover(0,0,601); m.hover(0,-120,602);
    check(m.selected()==0, "rearm enables leaf hover");
    check(m.back() && !m.nested() && m.selected()==-1, "explicit back");
    m.hover(120,0,700); check(m.selected()==-1, "back also needs rearm");
    m.hover(0,0,701); m.hover(0,120,702);
    check(!m.confirm() && m.active() && m.nested(), "confirm submenu never dispatches descendant");
    m.hover(0,0,703); m.hover(0,-120,704);
    result=m.confirm(); check(result && result->text=="Ctrl+Z", "nested leaf");
    m.open(root,28,220); check(!m.back() && !m.active(), "back at root cancels");
    m.open(root,28,220); m.hover(0,-201,1); check(m.selected()==-1, "outside circle cancels");
    m.hover(0,-28,2); check(m.selected()==-1, "dead-zone inclusive");
    m.hover(0,-29,3); check(m.selected()==0, "outside dead zone selects");
    m.hover(100,-100,4); check(m.selected()==0, "angular hysteresis");
    m.hover(120,-95,5); check(m.selected()==1, "crossing boundary selects neighbor");
    m.close(); check(!m.active() && !m.hover(0,0,100), "explicit cancellation");
    for (int count=1;count<=8;++count) {
        auto menu=std::make_shared<Menu>(); menu->label="Test";
        for(int i=0;i<count;++i) menu->items.push_back({std::to_string(i),parse_chord("F13"),{}});
        m.open(menu,28,220);
        for(int i=0;i<count;++i) {
            const double angle=6.283185307179586*i/count;
            m.hover(std::sin(angle)*100,-std::cos(angle)*100,100);
            check(m.selected()==i,"1 to 8 segment geometry");
        }
    }
    for (int i=0;i<200;++i) { m.open(root,28,220); m.hover(0,-100,i); if(i%2) m.close(); else check(m.confirm().has_value(),"stress dispatch"); }
    m.close(); check(!m.active(),"stress closes");
}
}
int main() {
    try { shortcuts(); configuration(); navigation(); std::cout << checks << " checks passed\n"; return 0; }
    catch(const std::exception& e) { std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n'; return 1; }
}
