#include "app/menu/controls_devices.hpp"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <memory>
#include <string>

#include "app/race_bindings.hpp"

namespace rr::app {
namespace {

constexpr std::array kKeyboardSets{InputSet::Keys1, InputSet::Keys2, InputSet::Keys3};

std::size_t OtherPlayer(std::size_t player) {
    return player == 0 ? 1 : 0;
}

SettingChange DeviceChange(std::size_t player, InputSet set) {
    return {.key = std::string(kPlayerDeviceSettings.at(player).key),
            .value = std::string(InputSetName(set))};
}

}

DeviceImage ImageOf(SDL_JoystickType type) {
    switch (type) {
        case SDL_JOYSTICK_TYPE_WHEEL:
            return DeviceImage::Wheel;
        case SDL_JOYSTICK_TYPE_FLIGHT_STICK:
        case SDL_JOYSTICK_TYPE_ARCADE_STICK:
        case SDL_JOYSTICK_TYPE_THROTTLE:
            return DeviceImage::Joystick;
        default:
            return DeviceImage::Gamepad;
    }
}

std::vector<AttachedPad> AttachedPads() {
    int count = 0;
    const std::unique_ptr<SDL_JoystickID, decltype(&SDL_free)> ids(SDL_GetGamepads(&count),
                                                                   &SDL_free);
    std::vector<AttachedPad> pads;
    for (int index = 0; ids != nullptr && index < count; ++index) {
        const SDL_JoystickID id = ids.get()[index];
        pads.push_back({.id = id, .image = ImageOf(SDL_GetJoystickTypeForID(id))});
    }
    return pads;
}

std::vector<DeviceEntry> DeviceEntries(std::span<const AttachedPad> pads) {
    std::vector<DeviceEntry> entries;
    for (std::size_t index = 0; index < std::min(pads.size(), kPadSets.size()); ++index) {
        entries.push_back(
            {.set = kPadSets.at(index), .image = pads[index].image, .pad = pads[index].id});
    }
    for (const InputSet set : kKeyboardSets) {
        entries.push_back({.set = set, .image = DeviceImage::Keyboard});
    }
    return entries;
}

bool RebindsSteering(InputSet set) {
    return !PadOf(set).has_value();
}

std::vector<SettingChange> DeviceChanges(const PlayerSets& chosen, std::size_t player, InputSet set,
                                         std::size_t padCount) {
    const PlayerSets driving = AssignInputSets(chosen, padCount);
    const PlayerSets after = ChooseInputSet(driving, player, set, padCount);
    std::vector<SettingChange> changes;
    if (chosen.at(player) != set) {
        changes.push_back(DeviceChange(player, set));
    }
    const std::size_t other = OtherPlayer(player);
    if (after.at(other) != driving.at(other)) {
        changes.push_back(DeviceChange(other, after.at(other)));
    }
    return changes;
}

}
