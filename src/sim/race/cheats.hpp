#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace rr::sim {

enum class Cheat : std::uint16_t {
    IgnoreSurfaces = 1U << 0U,
    HoverTurbo = 1U << 1U,
    RedBricks = 1U << 2U,
    YellowBricks = 1U << 3U,
    GreenBricks = 1U << 4U,
    MirroredBoss = 1U << 5U,
    RedOnly = 1U << 6U,
    MaxPower = 1U << 7U,
    FastForward = 1U << 8U,
    NoWheels = 1U << 9U,
    NoChassis = 1U << 10U,
    NoDriver = 1U << 11U,
};

class Cheats {
public:
    Cheats() = default;
    explicit Cheats(std::uint16_t bits) : bits_(bits) {}

    [[nodiscard]] bool Has(Cheat cheat) const {
        return (bits_ & static_cast<std::uint16_t>(cheat)) != 0;
    }
    [[nodiscard]] bool IsEmpty() const { return bits_ == 0; }
    [[nodiscard]] std::uint16_t Bits() const { return bits_; }

    /// Toggles as the licence does, clearing what a cheat turned on excludes. False if not a cheat.
    bool EnterPlate(std::string_view plate);

    friend bool operator==(const Cheats&, const Cheats&) = default;

private:
    void Clear(Cheat cheat) { bits_ &= static_cast<std::uint16_t>(~static_cast<unsigned>(cheat)); }

    std::uint16_t bits_ = 0;
};

[[nodiscard]] std::string_view PlateOf(Cheat cheat);

/// Comma separated, in bit order.
[[nodiscard]] std::string CheatPlates(const Cheats& cheats);

/// Fails on an unknown plate or a repeat.
[[nodiscard]] std::optional<Cheats> ParseCheatPlates(std::string_view text);

}
