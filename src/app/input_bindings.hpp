#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
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
               "input.buttons.accelerate", "Up", "a,righttrigger"),
    BindingFor(sim::Action::Brake, "Brake", "input.keys.brake", "input.buttons.brake", "Down",
               "b,lefttrigger"),
    BindingFor(sim::Action::SteerLeft, "Steer left", "input.keys.steer_left",
               "input.buttons.steer_left", "Left", "dpleft"),
    BindingFor(sim::Action::SteerRight, "Steer right", "input.keys.steer_right",
               "input.buttons.steer_right", "Right", "dpright"),
    BindingFor(sim::Action::Drift, "Drift", "input.keys.drift", "input.buttons.drift", "Space",
               "rightshoulder,leftshoulder"),
    BindingFor(sim::Action::UsePowerup, "Use powerup", "input.keys.use_powerup",
               "input.buttons.use_powerup", "Return", "x"),
    BindingFor(sim::Action::Pause, "Pause", "input.keys.pause", "input.buttons.pause", "Escape",
               "start"),
    BindingFor(sim::Action::ToggleDisplay, "Toggle display", "input.keys.toggle_display",
               "input.buttons.toggle_display", "M", "back"),
};

/// Player 2's bindings in a two-player race. Player 1's pause keys pause for both.
inline constexpr std::array kPlayer2Bindings{
    BindingFor(sim::Action::Accelerate, "Player 2 accelerate", "input.p2.keys.accelerate",
               "input.p2.buttons.accelerate", "W", "a,righttrigger"),
    BindingFor(sim::Action::Brake, "Player 2 brake", "input.p2.keys.brake",
               "input.p2.buttons.brake", "X", "b,lefttrigger"),
    BindingFor(sim::Action::SteerLeft, "Player 2 steer left", "input.p2.keys.steer_left",
               "input.p2.buttons.steer_left", "A", "dpleft"),
    BindingFor(sim::Action::SteerRight, "Player 2 steer right", "input.p2.keys.steer_right",
               "input.p2.buttons.steer_right", "D", "dpright"),
    BindingFor(sim::Action::Drift, "Player 2 drift", "input.p2.keys.drift",
               "input.p2.buttons.drift", "Left Shift", "rightshoulder,leftshoulder"),
    BindingFor(sim::Action::UsePowerup, "Player 2 use powerup", "input.p2.keys.use_powerup",
               "input.p2.buttons.use_powerup", "Left Alt", "x"),
    BindingFor(sim::Action::Pause, "Player 2 pause", "input.p2.keys.pause",
               "input.p2.buttons.pause", "", "start"),
    BindingFor(sim::Action::ToggleDisplay, "Player 2 toggle display",
               "input.p2.keys.toggle_display", "input.p2.buttons.toggle_display", "E", "back"),
};

[[nodiscard]] std::span<const Binding<sim::Action>> RaceBindingsOf(std::size_t player);

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

inline constexpr std::int16_t kTriggerPressed = 16000;
inline constexpr SDL_Keycode kToggleOverlayKey = SDLK_F1;
inline constexpr SDL_Keycode kCyclePacingKey = SDLK_F9;
inline constexpr SDL_Keycode kScreenshotKey = SDLK_PRINTSCREEN;

void DeclareInputSettings(settings::Registry& registry);

/// SDL's name, with each comma spelled `Comma` so it fits a comma-separated list.
[[nodiscard]] std::string KeyBindingName(SDL_Keycode key);

/// Reads names from `KeyBindingName` and plain SDL names alike.
[[nodiscard]] SDL_Keycode KeyFromBindingName(std::string_view name);

/// The names in a bindings setting, trimmed, without empty ones.
[[nodiscard]] std::vector<std::string> SplitBindingNames(std::string_view list);

/// With two players, keys only player 2 binds drive player 2. Pads go in connection order.
class InputBindings {
public:
    explicit InputBindings(const settings::Registry& registry);

    /// Keeps the player count and the pad order, and lets go of everything held.
    void Rebind(const settings::Registry& registry, sim::InputLatch& latch);
    void SetPlayerCount(std::size_t count, sim::InputLatch& latch);
    void Handle(const SDL_Event& event, sim::InputLatch& latch);

private:
    template <typename Input, typename Action>
    struct Bound {
        Input input;
        Action action;
    };

    template <typename Input>
    struct PadInput {
        SDL_JoystickID pad;
        Input input;

        friend bool operator==(const PadInput&, const PadInput&) = default;
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
    void AddPad(SDL_JoystickID pad);
    void RemovePad(SDL_JoystickID pad, sim::InputLatch& latch);
    void LatchHeldActions(sim::InputLatch& latch) const;
    [[nodiscard]] std::size_t PlayerOfKey(SDL_Keycode key) const;
    [[nodiscard]] std::size_t PlayerOfPad(SDL_JoystickID pad) const;
    [[nodiscard]] bool IsHeld(std::size_t player, sim::Action action) const;

    BoundInputs<sim::Action> race_;
    BoundInputs<sim::MenuAction> menu_;
    BoundInputs<sim::Action> player2_;
    std::size_t playerCount_ = 1;
    std::vector<SDL_JoystickID> pads_;
    std::vector<SDL_Keycode> heldKeys_;
    std::vector<PadInput<SDL_GamepadButton>> heldButtons_;
    std::vector<PadInput<SDL_GamepadAxis>> heldTriggers_;
};

}
