#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/assert.hpp"
#include "core/vector3.hpp"
#include "sim/powerup/powerup_kind.hpp"

namespace rr::sim {

enum class WeaponEventKind : std::uint8_t {
    Fired,
    Struck,
    Landed,
    Ended,
};

/// `racer` is the shooter for `Fired` and the victim for `Struck`.
struct WeaponEvent {
    WeaponEventKind kind = WeaponEventKind::Fired;
    PowerupKind weapon = PowerupKind::Cannonball;
    std::uint8_t racer = 0;
    Vector3 position;

    friend bool operator==(const WeaponEvent&, const WeaponEvent&) = default;
};

class WeaponEvents {
public:
    explicit WeaponEvents(std::size_t capacity) { events_.reserve(capacity); }

    void Clear() { events_.clear(); }

    void Add(const WeaponEvent& event) {
        RR_ASSERT(events_.size() < events_.capacity());
        events_.push_back(event);
    }

    [[nodiscard]] std::span<const WeaponEvent> Events() const { return events_; }

private:
    std::vector<WeaponEvent> events_;
};

}
