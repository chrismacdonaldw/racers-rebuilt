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
constexpr std::int16_t kTriggerPressed = 16000;

std::vector<std::string> SplitNames(std::string_view list) {
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

void Latch(sim::InputLatch& latch, sim::Action action, bool isDown) {
    if (isDown) {
        latch.Press(action);
    } else {
        latch.Release(action);
    }
}

}

void DeclareInputSettings(settings::Registry& registry) {
    for (const ActionBinding& binding : kActionBindings) {
        registry.Declare(binding.keys);
        registry.Declare(binding.buttons);
    }
}

InputBindings::InputBindings(const settings::Registry& registry) {
    for (const ActionBinding& binding : kActionBindings) {
        for (const std::string& name : SplitNames(registry.Get(binding.keys))) {
            const SDL_Keycode key = SDL_GetKeyFromName(name.c_str());
            if (key == SDLK_UNKNOWN) {
                log::Warn("`{}` in {} is not a key, ignoring it", name, binding.keys.key);
                continue;
            }
            keys_.push_back({.input = key, .action = binding.action});
        }
        for (const std::string& name : SplitNames(registry.Get(binding.buttons))) {
            const SDL_GamepadButton button = SDL_GetGamepadButtonFromString(name.c_str());
            const SDL_GamepadAxis axis = SDL_GetGamepadAxisFromString(name.c_str());
            if (button != SDL_GAMEPAD_BUTTON_INVALID) {
                buttons_.push_back({.input = button, .action = binding.action});
            } else if (IsTrigger(axis)) {
                triggers_.push_back({.input = axis, .action = binding.action});
            } else {
                log::Warn("`{}` in {} is not a gamepad button, ignoring it", name,
                          binding.buttons.key);
            }
        }
    }
}

void InputBindings::Handle(const SDL_Event& event, sim::InputLatch& latch) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (event.key.repeat) {
                return;
            }
            for (const Bound<SDL_Keycode>& bound : keys_) {
                if (bound.input == event.key.key) {
                    Latch(latch, bound.action, event.type == SDL_EVENT_KEY_DOWN);
                }
            }
            return;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            for (const Bound<SDL_GamepadButton>& bound : buttons_) {
                if (bound.input == event.gbutton.button) {
                    Latch(latch, bound.action, event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
                }
            }
            return;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            HandleAxis(event.gaxis, latch);
            return;
        default:
            return;
    }
}

void InputBindings::HandleAxis(const SDL_GamepadAxisEvent& event, sim::InputLatch& latch) {
    const auto axis = static_cast<SDL_GamepadAxis>(event.axis);
    if (axis == SDL_GAMEPAD_AXIS_LEFTX) {
        latch.SetSteerLeftAxis(SteerLeftFromStick(event.value));
        return;
    }
    if (!IsTrigger(axis)) {
        return;
    }
    const bool isHeld = event.value >= kTriggerPressed;
    bool& wasHeld = isTriggerHeld_.at(static_cast<std::size_t>(axis));
    if (isHeld == wasHeld) {
        return;
    }
    wasHeld = isHeld;
    for (const Bound<SDL_GamepadAxis>& bound : triggers_) {
        if (bound.input == axis) {
            Latch(latch, bound.action, isHeld);
        }
    }
}

}
