#pragma once

#include <SDL3/SDL_joystick.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "app/device_assignment.hpp"
#include "app/scene.hpp"

namespace rr::app {

enum class DeviceImage : std::uint8_t { Joystick, Gamepad, Wheel, Keyboard };

inline constexpr std::array<std::string_view, 4> kDeviceImageNames{"joystick", "gamepad", "wheel",
                                                                   "keyboard"};

struct AttachedPad {
    SDL_JoystickID id = 0;
    DeviceImage image = DeviceImage::Gamepad;
};

struct DeviceEntry {
    InputSet set = InputSet::Keys1;
    DeviceImage image = DeviceImage::Keyboard;
    SDL_JoystickID pad = 0;
};

[[nodiscard]] DeviceImage ImageOf(SDL_JoystickType type);

/// In connection order. Asks SDL, so never call it from a scene's Tick.
[[nodiscard]] std::vector<AttachedPad> AttachedPads();

/// The carousel: up to two attached pads, then the three keyboard sets.
[[nodiscard]] std::vector<DeviceEntry> DeviceEntries(std::span<const AttachedPad> pads);

[[nodiscard]] bool RebindsSteering(InputSet set);

/// Stores `player`'s set, and moves the other player off it when they were driving with it.
[[nodiscard]] std::vector<SettingChange> DeviceChanges(const PlayerSets& chosen, std::size_t player,
                                                       InputSet set, std::size_t padCount);

}
