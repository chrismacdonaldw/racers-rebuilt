#pragma once

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

/// Input for exactly one tick. `pressed` and `released` cover everything since the previous tick.
/// `menuPressed` counts key repeats too, and replays leave it out.
struct InputFrame {
    std::bitset<kActionCount> held;
    std::bitset<kActionCount> pressed;
    std::bitset<kActionCount> released;
    std::int8_t steerLeftAxis = 0;
    std::bitset<kMenuActionCount> menuPressed;

    [[nodiscard]] bool IsHeld(Action action) const { return held.test(Index(action)); }
    [[nodiscard]] bool WasPressed(Action action) const { return pressed.test(Index(action)); }
    [[nodiscard]] bool WasReleased(Action action) const { return released.test(Index(action)); }
    [[nodiscard]] bool WasPressed(MenuAction action) const {
        return menuPressed.test(Index(action));
    }

    [[nodiscard]] static constexpr std::size_t Index(Action action) {
        return static_cast<std::size_t>(action);
    }
    [[nodiscard]] static constexpr std::size_t Index(MenuAction action) {
        return static_cast<std::size_t>(action);
    }

    friend bool operator==(const InputFrame&, const InputFrame&) = default;
};

}
