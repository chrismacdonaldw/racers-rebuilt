#include "app/menu/control_bindings.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "app/input_bindings.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kTurnLeftRow = 0;
constexpr std::size_t kAccelerateRow = 2;
constexpr std::size_t kBrakeRow = 3;
constexpr std::size_t kPowerupRow = 4;
constexpr std::size_t kCameraRow = 5;
constexpr std::size_t kPlayer1 = 0;
constexpr std::size_t kPlayer2 = 1;

settings::Registry DefaultRegistry() {
    settings::Registry registry;
    DeclareInputSettings(registry);
    return registry;
}

SDL_Event KeyDown(SDL_Keycode key, bool isRepeat = false) {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = key;
    event.key.down = true;
    event.key.repeat = isRepeat;
    return event;
}

PressedControl Pressed(const SDL_Event& event) {
    return PressedControlOf(event).value_or(PressedControl{});
}

SDL_Event Axis(SDL_GamepadAxis axis, std::int16_t value) {
    SDL_Event event{};
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.axis = static_cast<std::uint8_t>(axis);
    event.gaxis.value = value;
    return event;
}

TEST_CASE("the controls rows read each device's own bindings, rows without an action empty") {
    const settings::Registry registry = DefaultRegistry();

    const ControlNames keys = ReadControlNames(registry, kPlayer1, ControlsDevice::Keyboard);
    const ControlNames buttons = ReadControlNames(registry, kPlayer1, ControlsDevice::Gamepad);

    CHECK(keys[kTurnLeftRow] == std::vector<std::string>{"Left"});
    CHECK(buttons[kAccelerateRow] == std::vector<std::string>{"a", "righttrigger"});
    CHECK(keys[kCameraRow].empty());
}

TEST_CASE("a new binding replaces the row's list and comes off the device's other rows") {
    const ControlNames keys =
        ReadControlNames(DefaultRegistry(), kPlayer1, ControlsDevice::Keyboard);

    const std::vector<SettingChange> changes =
        RebindChanges(keys, kPlayer1, ControlsDevice::Keyboard, kBrakeRow, "up");

    REQUIRE(changes.size() == 2);
    CHECK(changes[0].key == "input.keys.accelerate");
    CHECK(changes[0].value == settings::SettingValue{std::string()});
    CHECK(changes[1].key == "input.keys.brake");
    CHECK(changes[1].value == settings::SettingValue{std::string("up")});
}

TEST_CASE("binding a row its one input again changes nothing") {
    const ControlNames keys =
        ReadControlNames(DefaultRegistry(), kPlayer1, ControlsDevice::Keyboard);

    CHECK(RebindChanges(keys, kPlayer1, ControlsDevice::Keyboard, kPowerupRow, "Return").empty());
}

TEST_CASE("player 2's rows read and rebind player 2's own settings") {
    const settings::Registry registry = DefaultRegistry();
    const ControlNames keys = ReadControlNames(registry, kPlayer2, ControlsDevice::Keyboard);
    const ControlNames buttons = ReadControlNames(registry, kPlayer2, ControlsDevice::Gamepad);

    const std::vector<SettingChange> changes =
        RebindChanges(buttons, kPlayer2, ControlsDevice::Gamepad, kBrakeRow, "righttrigger");

    CHECK(keys[kAccelerateRow] == std::vector<std::string>{"W"});
    CHECK(buttons[kBrakeRow] == std::vector<std::string>{"b", "lefttrigger"});
    REQUIRE(changes.size() == 2);
    CHECK(changes[0].key == "input.p2.buttons.accelerate");
    CHECK(changes[0].value == settings::SettingValue{std::string("a")});
    CHECK(changes[1].key == "input.p2.buttons.brake");
    CHECK(changes[1].value == settings::SettingValue{std::string("righttrigger")});
}

TEST_CASE("pressed keys are named as the settings name them, the original's system keys refused") {
    const PressedControl letter = Pressed(KeyDown(SDLK_J));
    const PressedControl escape = Pressed(KeyDown(SDLK_ESCAPE));

    CHECK(letter.device == ControlsDevice::Keyboard);
    CHECK(letter.name == "J");
    CHECK_FALSE(letter.isRefused);
    CHECK(escape.isRefused);
    CHECK_FALSE(PressedControlOf(KeyDown(SDLK_J, true)));
}

TEST_CASE("comma keys, which the original binds, are named so the list keeps them whole") {
    const PressedControl comma = Pressed(KeyDown(SDLK_COMMA));
    const PressedControl keypad = Pressed(KeyDown(SDLK_KP_COMMA));

    CHECK(comma.name == "Comma");
    CHECK_FALSE(comma.isRefused);
    CHECK(keypad.name == "Keypad Comma");
    CHECK(KeyFromBindingName(comma.name) == SDLK_COMMA);
    CHECK(KeyFromBindingName(keypad.name) == SDLK_KP_COMMA);
    CHECK(SplitBindingNames("Comma,Keypad Comma") ==
          std::vector<std::string>{"Comma", "Keypad Comma"});
}

TEST_CASE("pressed gamepad buttons and triggers are named for the gamepad set") {
    SDL_Event button{};
    button.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    button.gbutton.button = static_cast<std::uint8_t>(SDL_GAMEPAD_BUTTON_SOUTH);
    button.gbutton.down = true;

    const PressedControl south = Pressed(button);
    const PressedControl trigger = Pressed(Axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, kTriggerPressed));

    CHECK(south.device == ControlsDevice::Gamepad);
    CHECK(south.name == "a");
    CHECK(trigger.name == "lefttrigger");
    CHECK_FALSE(PressedControlOf(Axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 100)));
    CHECK_FALSE(PressedControlOf(Axis(SDL_GAMEPAD_AXIS_LEFTX, kTriggerPressed)));
}

}
}
