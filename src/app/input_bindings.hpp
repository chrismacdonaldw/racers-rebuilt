#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/binding_categories.hpp"
#include "app/device_assignment.hpp"
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
    return {
        .action = action,
        .keys = {.key = keysKey, .defaultValue = keys, .label = name, .category = kKeys1},
        .buttons = {.key = buttonsKey, .defaultValue = buttons, .label = name, .category = kPad1}};
}

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

/// Each player drives only from their assigned set; a key both players bind drives player 1.
class InputBindings {
public:
    explicit InputBindings(const settings::Registry& registry);

    /// Keeps the player count and the pad order, and lets go of everything held.
    void Rebind(const settings::Registry& registry, sim::InputLatch& latch);
    void SetPlayerCount(std::size_t count, sim::InputLatch& latch);
    void Handle(const SDL_Event& event, sim::InputLatch& latch);

    [[nodiscard]] const PlayerSets& AssignedSets() const { return assigned_; }

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
    static void AddKeys(BoundInputs<Action>& bound, Action action,
                        const settings::Registry& registry,
                        const settings::SettingSpec<std::string_view>& spec);
    template <typename Action>
    static void AddButtons(BoundInputs<Action>& bound, Action action,
                           const settings::Registry& registry,
                           const settings::SettingSpec<std::string_view>& spec);

    void Load(const settings::Registry& registry);
    void AssignSets();
    void HandleKey(const SDL_KeyboardEvent& event, sim::InputLatch& latch);
    void HandleButton(const SDL_GamepadButtonEvent& event, sim::InputLatch& latch);
    void HandleAxis(const SDL_GamepadAxisEvent& event, sim::InputLatch& latch);
    void AddPad(SDL_JoystickID pad, sim::InputLatch& latch);
    void RemovePad(SDL_JoystickID pad, sim::InputLatch& latch);
    void LatchHeldActions(sim::InputLatch& latch) const;
    [[nodiscard]] bool Drives(std::size_t player, InputSet set) const;
    [[nodiscard]] bool IsDriven(InputSet set) const;
    [[nodiscard]] std::optional<std::size_t> PlayerOfPad(SDL_JoystickID pad) const;
    [[nodiscard]] bool IsHeld(std::size_t player, sim::Action action) const;
    [[nodiscard]] bool IsHeldInSet(InputSet set, std::size_t player, sim::Action action) const;
    [[nodiscard]] const BoundInputs<sim::Action>& Set(InputSet set) const;

    BoundInputs<sim::MenuAction> menu_;
    std::array<BoundInputs<sim::Action>, kInputSets.size()> sets_;
    PlayerSets chosen_ = kDefaultPlayerSets;
    PlayerSets assigned_ = kDefaultPlayerSets;
    bool isAnyDevice_ = false;
    std::size_t playerCount_ = 1;
    std::vector<SDL_JoystickID> pads_;
    std::vector<SDL_Keycode> heldKeys_;
    std::vector<PadInput<SDL_GamepadButton>> heldButtons_;
    std::vector<PadInput<SDL_GamepadAxis>> heldTriggers_;
};

}
