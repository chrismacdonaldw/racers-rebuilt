#pragma once

#include <SDL3/SDL_events.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/device_assignment.hpp"
#include "app/scene.hpp"
#include "settings/registry.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

enum class ControlsDevice : std::uint8_t { Keyboard, Gamepad };

inline constexpr std::size_t kControlRows = 9;

/// The original's rows, top to bottom.
inline constexpr std::array<sim::Action, kControlRows> kControlActions{
    sim::Action::SteerLeft,     sim::Action::SteerRight, sim::Action::Accelerate,
    sim::Action::Brake,         sim::Action::UsePowerup, sim::Action::ChangeCamera,
    sim::Action::ToggleDisplay, sim::Action::Drift,      sim::Action::LookBack,
};

using ControlNames = std::array<std::vector<std::string>, kControlRows>;

struct PressedControl {
    ControlsDevice device = ControlsDevice::Keyboard;
    SDL_JoystickID pad = 0;
    std::string name;
    bool isRefused = false;
};

[[nodiscard]] ControlsDevice DeviceOf(InputSet set);

[[nodiscard]] ControlNames ReadControlNames(const settings::Registry& settings, InputSet set);

/// Binds `input` alone to `row`'s action and takes it off the set's other rows.
[[nodiscard]] std::vector<SettingChange> RebindChanges(const ControlNames& names, InputSet set,
                                                       std::size_t row, std::string_view input);

/// Keys the original won't bind, and the port's own keys, come back refused.
[[nodiscard]] std::optional<PressedControl> PressedControlOf(const SDL_Event& event);

}
