#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>

#include <array>
#include <string_view>
#include <vector>

#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"
#include "sim/input_frame.hpp"
#include "sim/input_latch.hpp"

namespace rr::app {

struct ActionBinding {
    sim::Action action;
    settings::SettingSpec<std::string_view> keys;
    settings::SettingSpec<std::string_view> buttons;
};

[[nodiscard]] constexpr ActionBinding BindingFor(sim::Action action, std::string_view name,
                                                 std::string_view keysKey,
                                                 std::string_view buttonsKey, std::string_view keys,
                                                 std::string_view buttons) {
    return {.action = action,
            .keys = {.key = keysKey, .defaultValue = keys, .label = name, .category = "Keyboard"},
            .buttons = {
                .key = buttonsKey, .defaultValue = buttons, .label = name, .category = "Gamepad"}};
}

/// Values are comma-separated SDL key names, or SDL gamepad button and trigger names.
inline constexpr std::array kActionBindings{
    BindingFor(sim::Action::Accelerate, "Accelerate", "input.keys.accelerate",
               "input.buttons.accelerate", "Up,W", "a,righttrigger"),
    BindingFor(sim::Action::Brake, "Brake", "input.keys.brake", "input.buttons.brake", "Down,S",
               "b,lefttrigger"),
    BindingFor(sim::Action::SteerLeft, "Steer left", "input.keys.steer_left",
               "input.buttons.steer_left", "Left,A", "dpleft"),
    BindingFor(sim::Action::SteerRight, "Steer right", "input.keys.steer_right",
               "input.buttons.steer_right", "Right,D", "dpright"),
    BindingFor(sim::Action::Drift, "Drift", "input.keys.drift", "input.buttons.drift",
               "Left Shift,Right Shift", "rightshoulder,leftshoulder"),
    BindingFor(sim::Action::UsePowerup, "Use powerup", "input.keys.use_powerup",
               "input.buttons.use_powerup", "Space", "x"),
    BindingFor(sim::Action::Pause, "Pause", "input.keys.pause", "input.buttons.pause", "Escape",
               "start"),
};

void DeclareInputSettings(settings::Registry& registry);

class InputBindings {
public:
    explicit InputBindings(const settings::Registry& registry);

    void Handle(const SDL_Event& event, sim::InputLatch& latch);

private:
    template <typename Input>
    struct Bound {
        Input input;
        sim::Action action;
    };

    void HandleAxis(const SDL_GamepadAxisEvent& event, sim::InputLatch& latch);

    std::vector<Bound<SDL_Keycode>> keys_;
    std::vector<Bound<SDL_GamepadButton>> buttons_;
    std::vector<Bound<SDL_GamepadAxis>> triggers_;
    std::array<bool, SDL_GAMEPAD_AXIS_COUNT> isTriggerHeld_{};
};

}
