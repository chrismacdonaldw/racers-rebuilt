#include "app/input_bindings.hpp"

#include <doctest/doctest.h>

#include <cstdint>

namespace rr::app {
namespace {

settings::Registry DefaultRegistry() {
    settings::Registry registry;
    DeclareInputSettings(registry);
    return registry;
}

SDL_Event Key(SDL_EventType type, SDL_Keycode key, bool isRepeat = false) {
    SDL_Event event{};
    event.type = type;
    event.key.key = key;
    event.key.down = type == SDL_EVENT_KEY_DOWN;
    event.key.repeat = isRepeat;
    return event;
}

SDL_Event Button(SDL_EventType type, SDL_GamepadButton button, SDL_JoystickID pad = 0) {
    SDL_Event event{};
    event.type = type;
    event.gbutton.which = pad;
    event.gbutton.button = static_cast<std::uint8_t>(button);
    event.gbutton.down = type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    return event;
}

SDL_Event Axis(SDL_GamepadAxis axis, std::int16_t value, SDL_JoystickID pad = 0) {
    SDL_Event event{};
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.which = pad;
    event.gaxis.axis = static_cast<std::uint8_t>(axis);
    event.gaxis.value = value;
    return event;
}

SDL_Event Pad(SDL_EventType type, SDL_JoystickID pad) {
    SDL_Event event{};
    event.type = type;
    event.gdevice.which = pad;
    return event;
}

constexpr SDL_JoystickID kFirstPad = 7;
constexpr SDL_JoystickID kSecondPad = 3;

void ConnectTwoPads(InputBindings& bindings, sim::InputLatch& latch) {
    bindings.Handle(Pad(SDL_EVENT_GAMEPAD_ADDED, kFirstPad), latch);
    bindings.Handle(Pad(SDL_EVENT_GAMEPAD_ADDED, kSecondPad), latch);
}

TEST_CASE("default keys drive and repeats are ignored") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;

    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_UP), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_LEFT), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_UP, SDLK_LEFT), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_SPACE), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_DOWN, true), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_F1), latch);
    const sim::InputFrame frame = latch.TakeFrame();

    CHECK(frame.players[0].IsHeld(sim::Action::Accelerate));
    CHECK(frame.players[0].IsHeld(sim::Action::Drift));
    CHECK(frame.players[0].WasPressed(sim::Action::SteerLeft));
    CHECK(frame.players[0].WasReleased(sim::Action::SteerLeft));
    CHECK(frame.players[0].held.count() == 2);
}

TEST_CASE("an action stays held while another of its inputs is still down") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;

    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_UP), latch);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 30000), latch);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 100), latch);
    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_SOUTH), latch);
    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_UP, SDL_GAMEPAD_BUTTON_SOUTH), latch);
    const sim::InputFrame held = latch.TakeFrame();

    CHECK(held.players[0].IsHeld(sim::Action::Accelerate));
    CHECK_FALSE(held.players[0].WasReleased(sim::Action::Accelerate));

    bindings.Handle(Key(SDL_EVENT_KEY_UP, SDLK_UP), latch);
    CHECK(latch.TakeFrame().players[0].WasReleased(sim::Action::Accelerate));
}

TEST_CASE("disconnecting a gamepad releases its buttons") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;

    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_SOUTH), latch);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 30000), latch);
    SDL_Event removed{};
    removed.type = SDL_EVENT_GAMEPAD_REMOVED;
    bindings.Handle(removed, latch);
    const sim::InputFrame frame = latch.TakeFrame();

    CHECK_FALSE(frame.players[0].IsHeld(sim::Action::Accelerate));
    CHECK_FALSE(frame.players[0].IsHeld(sim::Action::Brake));
}

TEST_CASE("keys come from the settings and unknown names are skipped") {
    settings::Registry registry = DefaultRegistry();
    registry.Set(kActionBindings[0].keys, "Nonsense Key, J");
    InputBindings bindings(registry);
    sim::InputLatch latch;

    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_UP), latch);
    CHECK_FALSE(latch.TakeFrame().players[0].IsHeld(kActionBindings[0].action));
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_J), latch);
    CHECK(latch.TakeFrame().players[0].IsHeld(kActionBindings[0].action));
}

TEST_CASE("gamepad buttons and triggers press their actions") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;

    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER),
                    latch);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 30000), latch);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 30000), latch);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 100), latch);
    const sim::InputFrame frame = latch.TakeFrame();

    CHECK(frame.players[0].IsHeld(sim::Action::Drift));
    CHECK(frame.players[0].IsHeld(sim::Action::Accelerate));
    CHECK(frame.players[0].WasPressed(sim::Action::Brake));
    CHECK_FALSE(frame.players[0].IsHeld(sim::Action::Brake));
}

TEST_CASE("the left stick steers left as positive with a dead zone") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;

    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_LEFTX, -32768), latch);
    CHECK(latch.TakeFrame().players[0].steerLeftAxis == 127);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_LEFTX, 16384), latch);
    CHECK(latch.TakeFrame().players[0].steerLeftAxis == -64);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_LEFTX, 4000), latch);
    CHECK(latch.TakeFrame().players[0].steerLeftAxis == 0);
}

TEST_CASE("disconnecting a gamepad centres the stick") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_LEFTX, -32768), latch);
    SDL_Event removed{};
    removed.type = SDL_EVENT_GAMEPAD_REMOVED;

    bindings.Handle(removed, latch);

    CHECK(latch.TakeFrame().players[0].steerLeftAxis == 0);
}

TEST_CASE("menu keys press on every repeat and gamepad buttons press menu actions") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;

    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_DOWN), latch);
    CHECK(latch.TakeFrame().WasPressed(sim::MenuAction::Down));
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_DOWN, true), latch);
    const sim::InputFrame repeat = latch.TakeFrame();
    CHECK(repeat.WasPressed(sim::MenuAction::Down));
    CHECK_FALSE(repeat.players[0].WasPressed(sim::Action::Brake));

    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_SOUTH), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_UP, SDLK_RETURN), latch);
    const sim::InputFrame frame = latch.TakeFrame();
    CHECK(frame.WasPressed(sim::MenuAction::Confirm));
    CHECK(frame.menuPressed.count() == 1);
}

TEST_CASE("with two players, player 2's keys drive player 2") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;
    bindings.SetPlayerCount(2, latch);

    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_W), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_X), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_LEFT), latch);
    const sim::InputFrame frame = latch.TakeFrame();

    CHECK(frame.players[1].IsHeld(sim::Action::Accelerate));
    CHECK(frame.players[1].IsHeld(sim::Action::Brake));
    CHECK(frame.players[0].IsHeld(sim::Action::SteerLeft));
    CHECK(frame.players[0].held.count() == 1);
    CHECK(frame.players[1].held.count() == 2);
}

TEST_CASE("a key both players bind drives player 1") {
    settings::Registry registry = DefaultRegistry();
    registry.Set(kPlayer2KeyBindings[0].keys, "Up");
    InputBindings bindings(registry);
    sim::InputLatch latch;
    bindings.SetPlayerCount(2, latch);

    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_UP), latch);
    const sim::InputFrame frame = latch.TakeFrame();

    CHECK(frame.players[0].IsHeld(sim::Action::Accelerate));
    CHECK(frame.players[1] == sim::PlayerInput{});
}

TEST_CASE("with one player, player 2's keys do nothing") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;

    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_W), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_X), latch);
    const sim::InputFrame frame = latch.TakeFrame();

    CHECK(frame == sim::InputFrame{});
}

TEST_CASE("pads drive players in the order they connected") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;
    ConnectTwoPads(bindings, latch);
    bindings.SetPlayerCount(2, latch);

    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_SOUTH, kSecondPad),
                    latch);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 30000, kSecondPad), latch);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_LEFTX, -32768, kSecondPad), latch);
    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_EAST, kFirstPad),
                    latch);
    const sim::InputFrame frame = latch.TakeFrame();

    CHECK(frame.players[1].IsHeld(sim::Action::Accelerate));
    CHECK(frame.players[1].steerLeftAxis == 127);
    CHECK(frame.players[0].IsHeld(sim::Action::Brake));
    CHECK_FALSE(frame.players[0].IsHeld(sim::Action::Accelerate));
    CHECK(frame.players[0].steerLeftAxis == 0);
}

TEST_CASE("with one player, every pad drives player 1") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;
    ConnectTwoPads(bindings, latch);

    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_SOUTH, kSecondPad),
                    latch);
    const sim::InputFrame frame = latch.TakeFrame();

    CHECK(frame.players[0].IsHeld(sim::Action::Accelerate));
    CHECK(frame.players[1] == sim::PlayerInput{});
}

TEST_CASE("changing the player count moves what is held to its new player") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;
    ConnectTwoPads(bindings, latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_W), latch);
    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_EAST, kSecondPad),
                    latch);
    static_cast<void>(latch.TakeFrame());

    bindings.SetPlayerCount(2, latch);
    const sim::InputFrame frame = latch.TakeFrame();

    CHECK(frame.players[0].WasReleased(sim::Action::Brake));
    CHECK(frame.players[1].WasPressed(sim::Action::Accelerate));
    CHECK(frame.players[1].IsHeld(sim::Action::Brake));
}

TEST_CASE("disconnecting one pad releases only its player and passes the next pad up") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;
    ConnectTwoPads(bindings, latch);
    bindings.SetPlayerCount(2, latch);
    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_SOUTH, kFirstPad),
                    latch);
    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_EAST, kSecondPad),
                    latch);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_LEFTX, -32768, kSecondPad), latch);
    REQUIRE(latch.TakeFrame().players[1].steerLeftAxis != 0);

    bindings.Handle(Pad(SDL_EVENT_GAMEPAD_REMOVED, kFirstPad), latch);
    const sim::InputFrame frame = latch.TakeFrame();

    CHECK_FALSE(frame.players[0].IsHeld(sim::Action::Accelerate));
    CHECK(frame.players[0].IsHeld(sim::Action::Brake));
    CHECK(frame.players[1].WasReleased(sim::Action::Brake));
    CHECK(frame.players[1].steerLeftAxis == 0);
}

TEST_CASE("rebinding keeps the player count and pad order but lets go of held inputs") {
    settings::Registry registry = DefaultRegistry();
    InputBindings bindings(registry);
    sim::InputLatch latch;
    ConnectTwoPads(bindings, latch);
    bindings.SetPlayerCount(2, latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_UP), latch);
    static_cast<void>(latch.TakeFrame());
    registry.Set(kPlayer2KeyBindings[0].keys, "I");

    bindings.Rebind(registry, latch);
    CHECK(latch.TakeFrame().players[0].WasReleased(sim::Action::Accelerate));
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_I), latch);
    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_EAST, kSecondPad),
                    latch);
    const sim::InputFrame frame = latch.TakeFrame();

    CHECK(frame.players[1].IsHeld(kPlayer2KeyBindings[0].action));
    CHECK(frame.players[1].IsHeld(sim::Action::Brake));
}

}
}
