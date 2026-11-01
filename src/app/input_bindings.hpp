#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>

#include <array>
#include <span>
#include <string_view>
#include <vector>

#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"
#include "sim/input_frame.hpp"
#include "sim/input_latch.hpp"

namespace rr::app {

template <typename Action>
struct Binding {
    Action action;
    settings::SettingSpec<std::string_view> keys;
    settings::SettingSpec<std::string_view> buttons;
};

template <typename Action>
[[nodiscard]] constexpr Binding<Action> BindingFor(Action action, std::string_view name,
                                                   std::string_view keysKey,
                                                   std::string_view buttonsKey,
                                                   std::string_view keys,
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

inline constexpr std::array kMenuBindings{
    BindingFor(sim::MenuAction::Up, "Menu up", "input.keys.menu_up", "input.buttons.menu_up", "Up",
               "dpup"),
    BindingFor(sim::MenuAction::Down, "Menu down", "input.keys.menu_down",
               "input.buttons.menu_down", "Down", "dpdown"),
    BindingFor(sim::MenuAction::Left, "Menu left", "input.keys.menu_left",
               "input.buttons.menu_left", "Left", "dpleft"),
    BindingFor(sim::MenuAction::Right, "Menu right", "input.keys.menu_right",
               "input.buttons.menu_right", "Right", "dpright"),
    BindingFor(sim::MenuAction::Confirm, "Menu confirm", "input.keys.menu_confirm",
               "input.buttons.menu_confirm", "Return,Space", "a"),
    BindingFor(sim::MenuAction::Back, "Menu back", "input.keys.menu_back",
               "input.buttons.menu_back", "Escape,Backspace", "b"),
};

void DeclareInputSettings(settings::Registry& registry);

/// An action stays held while any of its keys, buttons or triggers is down.
class InputBindings {
public:
    explicit InputBindings(const settings::Registry& registry);

    void Handle(const SDL_Event& event, sim::InputLatch& latch);

private:
    template <typename Input, typename Action>
    struct Bound {
        Input input;
        Action action;
    };

    template <typename Action>
    struct BoundInputs {
        std::vector<Bound<SDL_Keycode, Action>> keys;
        std::vector<Bound<SDL_GamepadButton, Action>> buttons;
        std::vector<Bound<SDL_GamepadAxis, Action>> triggers;
    };

    template <typename Action>
    static BoundInputs<Action> Bind(const settings::Registry& registry,
                                    std::span<const Binding<Action>> bindings);

    void HandleKey(const SDL_KeyboardEvent& event, sim::InputLatch& latch);
    void HandleButton(const SDL_GamepadButtonEvent& event, sim::InputLatch& latch);
    void HandleAxis(const SDL_GamepadAxisEvent& event, sim::InputLatch& latch);
    void LatchHeldActions(sim::InputLatch& latch) const;
    [[nodiscard]] bool IsHeld(sim::Action action) const;

    BoundInputs<sim::Action> race_;
    BoundInputs<sim::MenuAction> menu_;
    std::vector<SDL_Keycode> heldKeys_;
    std::vector<SDL_GamepadButton> heldButtons_;
    std::array<bool, SDL_GAMEPAD_AXIS_COUNT> isTriggerHeld_{};
};

}
