#include "app/input_bindings.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr float kStickDeadZone = 8000.0F;
constexpr float kAxisMax = 32767.0F;
constexpr float kSteerAxisMax = 127.0F;
constexpr std::string_view kComma = "Comma";

bool IsTrigger(SDL_GamepadAxis axis) {
    return axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
}

std::int8_t SteerLeftFromStick(std::int16_t value) {
    const auto raw = static_cast<float>(value);
    if (std::abs(raw) < kStickDeadZone) {
        return 0;
    }
    const float left = std::clamp(-raw / kAxisMax, -1.0F, 1.0F);
    return static_cast<std::int8_t>(std::lround(left * kSteerAxisMax));
}

template <typename Input, typename Bound>
void PressMenuMatches(const std::vector<Bound>& bound, Input input, sim::InputLatch& latch) {
    for (const Bound& binding : bound) {
        if (binding.input == input) {
            latch.Press(binding.action);
        }
    }
}

std::vector<SDL_Keycode> KeysNamed(std::string_view list, std::string_view setting) {
    std::vector<SDL_Keycode> keys;
    for (const std::string& name : SplitBindingNames(list)) {
        const SDL_Keycode key = KeyFromBindingName(name);
        if (key == SDLK_UNKNOWN) {
            log::Warn("`{}` in {} is not a key, ignoring it", name, setting);
            continue;
        }
        keys.push_back(key);
    }
    return keys;
}

template <typename Input>
void SetHeld(std::vector<Input>& held, Input input, bool isDown) {
    const auto found = std::ranges::find(held, input);
    if (isDown && found == held.end()) {
        held.push_back(input);
    } else if (!isDown && found != held.end()) {
        held.erase(found);
    }
}

template <typename Bound, typename Held>
bool AnyHeld(const std::vector<Bound>& bound, sim::Action action, const Held& isHeld) {
    return std::ranges::any_of(bound, [&](const Bound& binding) {
        return binding.action == action && isHeld(binding.input);
    });
}

template <typename Bound, typename PadInput, typename OwnsPad>
bool AnyPadHeld(const std::vector<Bound>& bound, const std::vector<PadInput>& held,
                sim::Action action, const OwnsPad& ownsPad) {
    return std::ranges::any_of(held, [&](const PadInput& pressed) {
        return ownsPad(pressed.pad) && std::ranges::any_of(bound, [&](const Bound& binding) {
                   return binding.action == action && binding.input == pressed.input;
               });
    });
}

}

std::string KeyBindingName(SDL_Keycode key) {
    std::string name;
    for (const char letter : std::string_view(SDL_GetKeyName(key))) {
        if (letter == ',') {
            name += kComma;
        } else {
            name.push_back(letter);
        }
    }
    return name;
}

SDL_Keycode KeyFromBindingName(std::string_view name) {
    const std::string plain(name);
    const SDL_Keycode key = SDL_GetKeyFromName(plain.c_str());
    const std::size_t comma = plain.find(kComma);
    if (key != SDLK_UNKNOWN || comma == std::string::npos) {
        return key;
    }
    const std::string spelled = plain.substr(0, comma) + ',' + plain.substr(comma + kComma.size());
    return SDL_GetKeyFromName(spelled.c_str());
}

std::vector<std::string> SplitBindingNames(std::string_view list) {
    std::vector<std::string> names;
    while (!list.empty()) {
        const std::size_t comma = list.find(',');
        std::string_view name = list.substr(0, comma);
        list = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);
        const std::size_t first = name.find_first_not_of(' ');
        if (first != std::string_view::npos) {
            name = name.substr(first, name.find_last_not_of(' ') - first + 1);
            names.emplace_back(name);
        }
    }
    return names;
}

void DeclareInputSettings(settings::Registry& registry) {
    const auto declare = [&registry](const auto& bindings) {
        for (const auto& binding : bindings) {
            registry.Declare(binding.keys);
            registry.Declare(binding.buttons);
        }
    };
    declare(kActionBindings);
    declare(kMenuBindings);
    declare(kPlayer2Bindings);
}

std::span<const Binding<sim::Action>> RaceBindingsOf(std::size_t player) {
    if (player == 0) {
        return kActionBindings;
    }
    return kPlayer2Bindings;
}

template <typename Action>
InputBindings::BoundInputs<Action> InputBindings::Bind(const settings::Registry& registry,
                                                       std::span<const Binding<Action>> bindings) {
    BoundInputs<Action> bound;
    for (const Binding<Action>& binding : bindings) {
        for (const SDL_Keycode key : KeysNamed(registry.Get(binding.keys), binding.keys.key)) {
            bound.keys.push_back({.input = key, .action = binding.action});
        }
        for (const std::string& name : SplitBindingNames(registry.Get(binding.buttons))) {
            const SDL_GamepadButton button = SDL_GetGamepadButtonFromString(name.c_str());
            const SDL_GamepadAxis axis = SDL_GetGamepadAxisFromString(name.c_str());
            if (button != SDL_GAMEPAD_BUTTON_INVALID) {
                bound.buttons.push_back({.input = button, .action = binding.action});
            } else if (IsTrigger(axis)) {
                bound.triggers.push_back({.input = axis, .action = binding.action});
            } else {
                log::Warn("`{}` in {} is not a gamepad button, ignoring it", name,
                          binding.buttons.key);
            }
        }
    }
    return bound;
}

InputBindings::InputBindings(const settings::Registry& registry)
    : race_(Bind<sim::Action>(registry, kActionBindings)),
      menu_(Bind<sim::MenuAction>(registry, kMenuBindings)),
      player2_(Bind<sim::Action>(registry, kPlayer2Bindings)) {}

void InputBindings::Rebind(const settings::Registry& registry, sim::InputLatch& latch) {
    race_ = Bind<sim::Action>(registry, kActionBindings);
    menu_ = Bind<sim::MenuAction>(registry, kMenuBindings);
    player2_ = Bind<sim::Action>(registry, kPlayer2Bindings);
    heldKeys_.clear();
    heldButtons_.clear();
    heldTriggers_.clear();
    latch.ReleaseAll();
}

void InputBindings::SetPlayerCount(std::size_t count, sim::InputLatch& latch) {
    const std::size_t clamped = std::clamp<std::size_t>(count, 1, sim::kMaxPlayers);
    if (clamped == playerCount_) {
        return;
    }
    playerCount_ = clamped;
    latch.ReleaseAll();
    LatchHeldActions(latch);
}

void InputBindings::Handle(const SDL_Event& event, sim::InputLatch& latch) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            HandleKey(event.key, latch);
            return;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            HandleButton(event.gbutton, latch);
            return;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            HandleAxis(event.gaxis, latch);
            return;
        case SDL_EVENT_GAMEPAD_ADDED:
            AddPad(event.gdevice.which);
            return;
        case SDL_EVENT_GAMEPAD_REMOVED:
            RemovePad(event.gdevice.which, latch);
            return;
        default:
            return;
    }
}

void InputBindings::HandleKey(const SDL_KeyboardEvent& event, sim::InputLatch& latch) {
    if (event.down) {
        PressMenuMatches(menu_.keys, event.key, latch);
    }
    if (!event.repeat) {
        SetHeld(heldKeys_, event.key, event.down);
        LatchHeldActions(latch);
    }
}

void InputBindings::HandleButton(const SDL_GamepadButtonEvent& event, sim::InputLatch& latch) {
    AddPad(event.which);
    const auto button = static_cast<SDL_GamepadButton>(event.button);
    if (event.down) {
        PressMenuMatches(menu_.buttons, button, latch);
    }
    SetHeld(heldButtons_, PadInput<SDL_GamepadButton>{.pad = event.which, .input = button},
            event.down);
    LatchHeldActions(latch);
}

void InputBindings::HandleAxis(const SDL_GamepadAxisEvent& event, sim::InputLatch& latch) {
    AddPad(event.which);
    const auto axis = static_cast<SDL_GamepadAxis>(event.axis);
    if (axis == SDL_GAMEPAD_AXIS_LEFTX) {
        latch.SetSteerLeftAxis(PlayerOfPad(event.which), SteerLeftFromStick(event.value));
        return;
    }
    if (!IsTrigger(axis)) {
        return;
    }
    const PadInput<SDL_GamepadAxis> trigger{.pad = event.which, .input = axis};
    const bool isHeld = event.value >= kTriggerPressed;
    if (isHeld == std::ranges::contains(heldTriggers_, trigger)) {
        return;
    }
    SetHeld(heldTriggers_, trigger, isHeld);
    if (isHeld) {
        PressMenuMatches(menu_.triggers, axis, latch);
    }
    LatchHeldActions(latch);
}

void InputBindings::AddPad(SDL_JoystickID pad) {
    if (!std::ranges::contains(pads_, pad)) {
        pads_.push_back(pad);
    }
}

void InputBindings::RemovePad(SDL_JoystickID pad, sim::InputLatch& latch) {
    std::erase_if(heldButtons_, [pad](const auto& held) { return held.pad == pad; });
    std::erase_if(heldTriggers_, [pad](const auto& held) { return held.pad == pad; });
    for (std::size_t player = 0; player < sim::kMaxPlayers; ++player) {
        latch.SetSteerLeftAxis(player, 0);
    }
    std::erase(pads_, pad);
    LatchHeldActions(latch);
}

void InputBindings::LatchHeldActions(sim::InputLatch& latch) const {
    for (std::size_t player = 0; player < sim::kMaxPlayers; ++player) {
        for (std::size_t i = 0; i < sim::kActionCount; ++i) {
            const auto action = static_cast<sim::Action>(i);
            if (IsHeld(player, action)) {
                latch.Press(player, action);
            } else {
                latch.Release(player, action);
            }
        }
    }
}

std::size_t InputBindings::PlayerOfKey(SDL_Keycode key) const {
    constexpr auto kKey = &Bound<SDL_Keycode, sim::Action>::input;
    const bool isPlayer2 = playerCount_ > 1 && !std::ranges::contains(race_.keys, key, kKey) &&
                           std::ranges::contains(player2_.keys, key, kKey);
    return isPlayer2 ? 1 : 0;
}

std::size_t InputBindings::PlayerOfPad(SDL_JoystickID pad) const {
    const auto found = std::ranges::find(pads_, pad);
    const auto index = static_cast<std::size_t>(found - pads_.begin());
    return index < playerCount_ ? index : 0;
}

bool InputBindings::IsHeld(std::size_t player, sim::Action action) const {
    const auto isKeyHeld = [&](SDL_Keycode key) {
        return PlayerOfKey(key) == player && std::ranges::contains(heldKeys_, key);
    };
    const auto ownsPad = [&](SDL_JoystickID pad) { return PlayerOfPad(pad) == player; };
    const BoundInputs<sim::Action>& bound = player == 0 ? race_ : player2_;
    return AnyHeld(bound.keys, action, isKeyHeld) ||
           AnyPadHeld(bound.buttons, heldButtons_, action, ownsPad) ||
           AnyPadHeld(bound.triggers, heldTriggers_, action, ownsPad);
}

}
