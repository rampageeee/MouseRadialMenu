#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace radial {
// Virtual-key numbers are stable Win32 values; the core has no Windows headers.
enum Modifier : unsigned { Alt = 1, Ctrl = 2, Shift = 4, Win = 8 };
struct Chord {
    unsigned modifiers{};
    std::uint16_t key{};
    std::string text;
    bool operator==(const Chord& other) const {
        return modifiers == other.modifiers && key == other.key;
    }
};
Chord parse_chord(std::string_view text);
std::vector<std::uint16_t> chord_keys(const Chord& chord);
std::string ascii_lower(std::string value);

enum class ActivationBehavior { Toggle, Hold };
enum class HoldEvent { Pass, Consume, Open, Release };
// Own only the trigger's primary key. Modifiers may already belong to the target.
// Cancellation invalidates the gesture but retains ownership through its key-up.
class HoldActivation {
public:
    HoldEvent event(const Chord& trigger, bool enabled, std::uint16_t key,
        bool up, bool repeat, unsigned modifiers);
    void cancel() { valid_ = false; }
    bool holding() const { return owned_key_ != 0; }
    bool valid() const { return valid_; }
    std::uint32_t session() const { return session_; }
private:
    std::uint16_t owned_key_ = 0;
    bool valid_ = false;
    std::uint32_t session_ = 0;
};

struct Menu;
struct Item {
    std::string label;
    std::optional<Chord> chord;
    std::shared_ptr<const Menu> submenu;
};
struct Menu { std::string label; std::vector<Item> items; };
struct Profile {
    std::string name;
    std::vector<std::string> executables;
    std::shared_ptr<const Menu> menu;
};
struct Config {
    Chord activation;
    ActivationBehavior activation_behavior = ActivationBehavior::Toggle;
    double dead_zone = 28;
    std::uint64_t dwell_ms = 220;
    bool show_shortcuts = true;
    std::vector<Profile> profiles;
    const Profile& match(std::string executable) const;
};
std::shared_ptr<const Config> load_config(const std::filesystem::path& directory);
// Candidate is parsed in isolation. Call commit only after OS hotkey registration succeeds.
class ConfigStore {
public:
    std::shared_ptr<const Config> current() const { return current_; }
    void commit(std::shared_ptr<const Config> candidate) { current_ = std::move(candidate); }
private:
    std::shared_ptr<const Config> current_;
};

class MenuState {
public:
    void open(std::shared_ptr<const Menu> root, double dead_zone, std::uint64_t dwell_ms);
    void close();
    bool hover(double x, double y, std::uint64_t now_ms);
    // Selecting a submenu enters it; it never dispatches a descendant on the same gesture.
    std::optional<Chord> confirm();
    // A hold gesture ends on release; a submenu is never confirmed by release.
    std::optional<Chord> release();
    bool back();
    bool active() const { return !stack_.empty(); }
    const Menu& menu() const { return *stack_.back(); }
    int selected() const { return selected_; }
    bool nested() const { return stack_.size() > 1; }
private:
    void enter(std::shared_ptr<const Menu> menu);
    std::vector<std::shared_ptr<const Menu>> stack_;
    int selected_ = -1;
    bool armed_ = true;
    double dead_zone_ = 28;
    std::uint64_t dwell_ms_ = 220;
    std::uint64_t hover_since_ = 0;
};
}
