#include "core.hpp"
#include <cmath>
#include <numbers>

namespace radial {
void MenuState::open(std::shared_ptr<const Menu> root, double dead_zone, std::uint64_t dwell_ms) {
    close();
    dead_zone_ = dead_zone;
    dwell_ms_ = dwell_ms;
    if (root && !root->items.empty()) stack_.push_back(std::move(root));
}
void MenuState::close() { stack_.clear(); selected_ = -1; armed_ = true; hover_since_ = 0; }
void MenuState::enter(std::shared_ptr<const Menu> menu) {
    stack_.push_back(std::move(menu));
    selected_ = -1;
    armed_ = false; // Return to the center before selecting a descendant.
    hover_since_ = 0;
}
bool MenuState::hover(double x, double y, std::uint64_t now_ms) {
    if (!active()) return false;
    const int before = selected_;
    const double radius = std::hypot(x, y);
    if (radius <= dead_zone_) {
        armed_ = true;
        selected_ = -1;
    } else if (armed_ && radius <= 200) {
        constexpr double tau = 2 * std::numbers::pi;
        double angle = std::atan2(x, -y);
        if (angle < 0) angle += tau;
        const auto count = menu().items.size();
        const double width = tau / static_cast<double>(count);
        int candidate = static_cast<int>(std::floor((angle + width / 2) / width)) % static_cast<int>(count);
        // Small angular hysteresis prevents boundary jitter from restarting dwell.
        if (selected_ >= 0 && candidate != selected_) {
            double distance = std::abs(std::remainder(angle - selected_ * width, tau));
            if (distance < width / 2 + 0.06) candidate = selected_;
        }
        selected_ = candidate;
    } else selected_ = -1;
    if (selected_ != before) hover_since_ = now_ms;
    if (selected_ >= 0 && menu().items[static_cast<std::size_t>(selected_)].submenu && now_ms - hover_since_ >= dwell_ms_) {
        enter(menu().items[static_cast<std::size_t>(selected_)].submenu);
        return true;
    }
    return selected_ != before;
}
std::optional<Chord> MenuState::confirm() {
    if (!active()) return {};
    if (selected_ < 0) { close(); return {}; }
    const auto& item = menu().items[static_cast<std::size_t>(selected_)];
    if (item.submenu) { enter(item.submenu); return {}; }
    auto result = item.chord;
    close();
    return result;
}
bool MenuState::back() {
    if (!nested()) { close(); return false; }
    stack_.pop_back(); selected_ = -1; armed_ = false; hover_since_ = 0;
    return true;
}
}
