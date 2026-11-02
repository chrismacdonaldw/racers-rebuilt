#pragma once

#include <SDL3/SDL_events.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/scene.hpp"
#include "settings/registry.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

enum class ControlsDevice : std::uint8_t { Keyboard, Gamepad };

inline constexpr std::size_t kControlRows = 9;

/// The original's rows, top to bottom. Camera, display and look back have no action yet.
inline constexpr std::array<std::optional<sim::Action>, kControlRows> kControlActions{
    sim::Action::SteerLeft, sim::Action::SteerRight, sim::Action::Accelerate,
    sim::Action::Brake,     sim::Action::UsePowerup, std::nullopt,
    std::nullopt,           sim::Action::Drift,      std::nullopt,
};

using ControlNames = std::array<std::vector<std::string>, kControlRows>;

struct PressedControl {
    ControlsDevice device = ControlsDevice::Keyboard;
    std::string name;
    bool isRefused = false;
};

[[nodiscard]] ControlNames ReadControlNames(const settings::Registry& settings,
                                            ControlsDevice device);

/// Binds `input` alone to `row`'s action and takes it off the device's other rows.
[[nodiscard]] std::vector<SettingChange> RebindChanges(const ControlNames& names,
                                                       ControlsDevice device, std::size_t row,
                                                       std::string_view input);

/// Keys the original won't bind, and the port's own keys, come back refused.
[[nodiscard]] std::optional<PressedControl> PressedControlOf(const SDL_Event& event);

}
