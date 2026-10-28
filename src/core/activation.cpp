#include "core.hpp"

namespace radial {
HoldEvent HoldActivation::event(const Chord& trigger, bool enabled, std::uint16_t key,
    bool up, bool repeat, unsigned modifiers) {
    if (owned_key_ && key == owned_key_) {
        if (!up) return HoldEvent::Consume;
        owned_key_ = 0;
        return HoldEvent::Release;
    }
    if (owned_key_ || !enabled || up || repeat || key != trigger.key || modifiers != trigger.modifiers)
        return HoldEvent::Pass;
    owned_key_ = key;
    valid_ = true;
    ++session_;
    return HoldEvent::Open;
}
}
