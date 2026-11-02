#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>

namespace rr::sim {

enum class Action : std::uint8_t {
    Accelerate,
    Brake,
    SteerLeft,
    SteerRight,
    UsePowerup,
    Pause,
    Drift,
    ToggleDisplay,
    Count,
};

inline constexpr std::size_t kActionCount = static_cast<std::size_t>(Action::Count);

enum class MenuAction : std::uint8_t {
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Back,
    Count,
};

inline constexpr std::size_t kMenuActionCount = static_cast<std::size_t>(MenuAction::Count);

inline constexpr std::size_t kMaxPlayers = 2;

/// `pressed` and `released` cover everything since the previous tick.
struct PlayerInput {
    std::bitset<kActionCount> held;
    std::bitset<kActionCount> pressed;
    std::bitset<kActionCount> released;
    std::int8_t steerLeftAxis = 0;

    [[nodiscard]] bool IsHeld(Action action) const { return held.test(Index(action)); }
    [[nodiscard]] bool WasPressed(Action action) const { return pressed.test(Index(action)); }
    [[nodiscard]] bool WasReleased(Action action) const { return released.test(Index(action)); }

    [[nodiscard]] static constexpr std::size_t Index(Action action) {
        return static_cast<std::size_t>(action);
    }

    friend bool operator==(const PlayerInput&, const PlayerInput&) = default;
};

/// Input for exactly one tick. `menuPressed` is shared by every player and counts key repeats too.
struct InputFrame {
    std::array<PlayerInput, kMaxPlayers> players;
    std::bitset<kMenuActionCount> menuPressed;

    [[nodiscard]] bool WasPressed(MenuAction action) const {
        return menuPressed.test(Index(action));
    }
    [[nodiscard]] bool WasAnyPressed(Action action) const {
        return std::ranges::any_of(
            players, [action](const PlayerInput& player) { return player.WasPressed(action); });
    }
    [[nodiscard]] bool IsAnyHeld(Action action) const {
        return std::ranges::any_of(
            players, [action](const PlayerInput& player) { return player.IsHeld(action); });
    }

    [[nodiscard]] static constexpr std::size_t Index(MenuAction action) {
        return static_cast<std::size_t>(action);
    }

    friend bool operator==(const InputFrame&, const InputFrame&) = default;
};

}
