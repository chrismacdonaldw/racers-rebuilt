#pragma once

#include <array>
#include <span>
#include <string_view>

#include "app/device_assignment.hpp"
#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

/// A keyboard set's values are SDL key names; a pad set's are gamepad button and trigger names.
struct RaceBinding {
    sim::Action action;
    settings::SettingSpec<std::string_view> inputs;
};

[[nodiscard]] constexpr RaceBinding RaceBindingFor(std::string_view category, sim::Action action,
                                                   std::string_view label, std::string_view key,
                                                   std::string_view inputs) {
    return {.action = action,
            .inputs = {.key = key, .defaultValue = inputs, .label = label, .category = category}};
}

inline constexpr std::string_view kKeys1 = "Keyboard";

inline constexpr std::array kKeys1Bindings{
    RaceBindingFor(kKeys1, sim::Action::Accelerate, "Accelerate", "input.keys.accelerate", "Up"),
    RaceBindingFor(kKeys1, sim::Action::Brake, "Brake", "input.keys.brake", "Down"),
    RaceBindingFor(kKeys1, sim::Action::SteerLeft, "Steer left", "input.keys.steer_left", "Left"),
    RaceBindingFor(kKeys1, sim::Action::SteerRight, "Steer right", "input.keys.steer_right",
                   "Right"),
    RaceBindingFor(kKeys1, sim::Action::Drift, "Drift", "input.keys.drift", "Space"),
    RaceBindingFor(kKeys1, sim::Action::UsePowerup, "Use powerup", "input.keys.use_powerup",
                   "Return"),
    RaceBindingFor(kKeys1, sim::Action::Pause, "Pause", "input.keys.pause", "Escape"),
    RaceBindingFor(kKeys1, sim::Action::ToggleDisplay, "Toggle display",
                   "input.keys.toggle_display", "M"),
    RaceBindingFor(kKeys1, sim::Action::ChangeCamera, "Change camera view",
                   "input.keys.change_camera", "Right Ctrl"),
    RaceBindingFor(kKeys1, sim::Action::LookBack, "Look backwards", "input.keys.look_back",
                   "Right Alt"),
};

inline constexpr std::string_view kKeys2 = "Keyboard 2";

inline constexpr std::array kKeys2Bindings{
    RaceBindingFor(kKeys2, sim::Action::Accelerate, "Accelerate", "input.keys2.accelerate", "W"),
    RaceBindingFor(kKeys2, sim::Action::Brake, "Brake", "input.keys2.brake", "X"),
    RaceBindingFor(kKeys2, sim::Action::SteerLeft, "Steer left", "input.keys2.steer_left", "A"),
    RaceBindingFor(kKeys2, sim::Action::SteerRight, "Steer right", "input.keys2.steer_right", "D"),
    RaceBindingFor(kKeys2, sim::Action::Drift, "Drift", "input.keys2.drift", "Left Shift"),
    RaceBindingFor(kKeys2, sim::Action::UsePowerup, "Use powerup", "input.keys2.use_powerup",
                   "Left Alt"),
    RaceBindingFor(kKeys2, sim::Action::Pause, "Pause", "input.keys2.pause", ""),
    RaceBindingFor(kKeys2, sim::Action::ToggleDisplay, "Toggle display",
                   "input.keys2.toggle_display", "E"),
    RaceBindingFor(kKeys2, sim::Action::ChangeCamera, "Change camera view",
                   "input.keys2.change_camera", "Q"),
    RaceBindingFor(kKeys2, sim::Action::LookBack, "Look backwards", "input.keys2.look_back", "Z"),
};

inline constexpr std::string_view kKeys3 = "Keyboard 3";

inline constexpr std::array kKeys3Bindings{
    RaceBindingFor(kKeys3, sim::Action::Accelerate, "Accelerate", "input.keys3.accelerate",
                   "Keypad 8"),
    RaceBindingFor(kKeys3, sim::Action::Brake, "Brake", "input.keys3.brake", "Keypad 2"),
    RaceBindingFor(kKeys3, sim::Action::SteerLeft, "Steer left", "input.keys3.steer_left",
                   "Keypad 4"),
    RaceBindingFor(kKeys3, sim::Action::SteerRight, "Steer right", "input.keys3.steer_right",
                   "Keypad 6"),
    RaceBindingFor(kKeys3, sim::Action::Drift, "Drift", "input.keys3.drift", "Keypad Enter"),
    RaceBindingFor(kKeys3, sim::Action::UsePowerup, "Use powerup", "input.keys3.use_powerup",
                   "Right Shift"),
    RaceBindingFor(kKeys3, sim::Action::Pause, "Pause", "input.keys3.pause", ""),
    RaceBindingFor(kKeys3, sim::Action::ToggleDisplay, "Toggle display",
                   "input.keys3.toggle_display", "Keypad 7"),
    RaceBindingFor(kKeys3, sim::Action::ChangeCamera, "Change camera view",
                   "input.keys3.change_camera", "Keypad 3"),
    RaceBindingFor(kKeys3, sim::Action::LookBack, "Look backwards", "input.keys3.look_back",
                   "Keypad 9"),
};

inline constexpr std::string_view kPad1 = "Gamepad";

inline constexpr std::array kPad1Bindings{
    RaceBindingFor(kPad1, sim::Action::Accelerate, "Accelerate", "input.buttons.accelerate",
                   "a,righttrigger"),
    RaceBindingFor(kPad1, sim::Action::Brake, "Brake", "input.buttons.brake", "b,lefttrigger"),
    RaceBindingFor(kPad1, sim::Action::SteerLeft, "Steer left", "input.buttons.steer_left",
                   "dpleft"),
    RaceBindingFor(kPad1, sim::Action::SteerRight, "Steer right", "input.buttons.steer_right",
                   "dpright"),
    RaceBindingFor(kPad1, sim::Action::Drift, "Drift", "input.buttons.drift",
                   "rightshoulder,leftshoulder"),
    RaceBindingFor(kPad1, sim::Action::UsePowerup, "Use powerup", "input.buttons.use_powerup", "x"),
    RaceBindingFor(kPad1, sim::Action::Pause, "Pause", "input.buttons.pause", "start"),
    RaceBindingFor(kPad1, sim::Action::ToggleDisplay, "Toggle display",
                   "input.buttons.toggle_display", "back"),
    RaceBindingFor(kPad1, sim::Action::ChangeCamera, "Change camera view",
                   "input.buttons.change_camera", "y"),
    RaceBindingFor(kPad1, sim::Action::LookBack, "Look backwards", "input.buttons.look_back",
                   "rightstick"),
};

inline constexpr std::string_view kPad2 = "Gamepad 2";

inline constexpr std::array kPad2Bindings{
    RaceBindingFor(kPad2, sim::Action::Accelerate, "Accelerate", "input.pad2.buttons.accelerate",
                   "a,righttrigger"),
    RaceBindingFor(kPad2, sim::Action::Brake, "Brake", "input.pad2.buttons.brake", "b,lefttrigger"),
    RaceBindingFor(kPad2, sim::Action::SteerLeft, "Steer left", "input.pad2.buttons.steer_left",
                   "dpleft"),
    RaceBindingFor(kPad2, sim::Action::SteerRight, "Steer right", "input.pad2.buttons.steer_right",
                   "dpright"),
    RaceBindingFor(kPad2, sim::Action::Drift, "Drift", "input.pad2.buttons.drift",
                   "rightshoulder,leftshoulder"),
    RaceBindingFor(kPad2, sim::Action::UsePowerup, "Use powerup", "input.pad2.buttons.use_powerup",
                   "x"),
    RaceBindingFor(kPad2, sim::Action::Pause, "Pause", "input.pad2.buttons.pause", "start"),
    RaceBindingFor(kPad2, sim::Action::ToggleDisplay, "Toggle display",
                   "input.pad2.buttons.toggle_display", "back"),
    RaceBindingFor(kPad2, sim::Action::ChangeCamera, "Change camera view",
                   "input.pad2.buttons.change_camera", "y"),
    RaceBindingFor(kPad2, sim::Action::LookBack, "Look backwards", "input.pad2.buttons.look_back",
                   "rightstick"),
};

[[nodiscard]] std::span<const RaceBinding> RaceBindingsOf(InputSet set);

/// Values are `InputSet` names such as `pad1` or `keys2`.
inline constexpr std::array<settings::SettingSpec<std::string_view>, sim::kMaxPlayers>
    kPlayerDeviceSettings{{
        {.key = "input.p1.device",
         .defaultValue = "pad1",
         .label = "Player 1 device",
         .category = "Controls"},
        {.key = "input.p2.device",
         .defaultValue = "pad2",
         .label = "Player 2 device",
         .category = "Controls"},
    }};

inline constexpr settings::SettingSpec<bool> kAnyDeviceSetting{
    .key = "input.p1.any_device",
    .defaultValue = false,
    .label = "Any device drives player 1",
    .category = "Controls"};

/// A device name that isn't a set leaves that player on their default.
[[nodiscard]] PlayerSets ChosenPlayerSets(const settings::Registry& registry);

}
