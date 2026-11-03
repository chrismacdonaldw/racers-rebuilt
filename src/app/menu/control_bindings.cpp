#include "app/menu/control_bindings.hpp"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <cctype>
#include <span>
#include <utility>

#include "app/input_bindings.hpp"
#include "app/race_bindings.hpp"

namespace rr::app {
namespace {

constexpr std::array kRefusedKeys{
    SDLK_ESCAPE, SDLK_PRINTSCREEN,  SDLK_SCROLLLOCK,  SDLK_PAUSE, SDLK_NUMLOCKCLEAR,
    SDLK_LGUI,   SDLK_RGUI,         SDLK_APPLICATION, SDLK_POWER, SDLK_SLEEP,
    SDLK_WAKE,   kToggleOverlayKey, kCyclePacingKey,
};

const settings::SettingSpec<std::string_view>* SpecFor(sim::Action action, InputSet set) {
    const std::span<const RaceBinding> bindings = RaceBindingsOf(set);
    const auto binding = std::ranges::find(bindings, action, &RaceBinding::action);
    if (binding == bindings.end()) {
        return nullptr;
    }
    return &binding->inputs;
}

bool IsSameName(std::string_view left, std::string_view right) {
    return std::ranges::equal(left, right, [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    });
}

std::string Joined(const std::vector<std::string>& names) {
    std::string list;
    for (const std::string& name : names) {
        list += list.empty() ? name : "," + name;
    }
    return list;
}

std::optional<PressedControl> NamedControl(PressedControl control) {
    if (control.name.empty()) {
        return std::nullopt;
    }
    return control;
}

std::string NameOf(const char* name) {
    return name != nullptr ? std::string(name) : std::string();
}

}

ControlsDevice DeviceOf(InputSet set) {
    return PadOf(set) ? ControlsDevice::Gamepad : ControlsDevice::Keyboard;
}

ControlNames ReadControlNames(const settings::Registry& settings, InputSet set) {
    ControlNames names;
    for (std::size_t row = 0; row < kControlRows; ++row) {
        if (const auto* spec = SpecFor(kControlActions.at(row), set)) {
            names.at(row) = SplitBindingNames(settings.Get(*spec));
        }
    }
    return names;
}

std::vector<SettingChange> RebindChanges(const ControlNames& names, InputSet set, std::size_t row,
                                         std::string_view input) {
    std::vector<SettingChange> changes;
    for (std::size_t other = 0; other < kControlRows; ++other) {
        const auto* spec = SpecFor(kControlActions.at(other), set);
        if (spec == nullptr) {
            continue;
        }
        std::vector<std::string> kept = names.at(other);
        if (other == row) {
            kept = {std::string(input)};
        } else {
            std::erase_if(kept, [&](const std::string& name) { return IsSameName(name, input); });
        }
        if (kept != names.at(other)) {
            changes.push_back({.key = std::string(spec->key), .value = Joined(kept)});
        }
    }
    return changes;
}

std::optional<PressedControl> PressedControlOf(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            if (event.key.repeat) {
                return std::nullopt;
            }
            return NamedControl({.device = ControlsDevice::Keyboard,
                                 .name = KeyBindingName(event.key.key),
                                 .isRefused = std::ranges::contains(kRefusedKeys, event.key.key)});
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            return NamedControl({.device = ControlsDevice::Gamepad,
                                 .pad = event.gbutton.which,
                                 .name = NameOf(SDL_GetGamepadStringForButton(
                                     static_cast<SDL_GamepadButton>(event.gbutton.button)))});
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            const auto axis = static_cast<SDL_GamepadAxis>(event.gaxis.axis);
            const bool isTrigger =
                axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
            if (!isTrigger || event.gaxis.value < kTriggerPressed) {
                return std::nullopt;
            }
            return NamedControl({.device = ControlsDevice::Gamepad,
                                 .pad = event.gaxis.which,
                                 .name = NameOf(SDL_GetGamepadStringForAxis(axis))});
        }
        default:
            return std::nullopt;
    }
}

}
