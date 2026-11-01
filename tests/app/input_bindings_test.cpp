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

SDL_Event Button(SDL_EventType type, SDL_GamepadButton button) {
    SDL_Event event{};
    event.type = type;
    event.gbutton.button = static_cast<std::uint8_t>(button);
    event.gbutton.down = type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    return event;
}

SDL_Event Axis(SDL_GamepadAxis axis, std::int16_t value) {
    SDL_Event event{};
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.axis = static_cast<std::uint8_t>(axis);
    event.gaxis.value = value;
    return event;
}

TEST_CASE("default keys drive and repeats are ignored") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;

    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_UP), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_A), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_UP, SDLK_A), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_LSHIFT), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_DOWN, true), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_F1), latch);
    const sim::InputFrame frame = latch.TakeFrame();

    CHECK(frame.IsHeld(sim::Action::Accelerate));
    CHECK(frame.IsHeld(sim::Action::Drift));
    CHECK(frame.WasPressed(sim::Action::SteerLeft));
    CHECK(frame.WasReleased(sim::Action::SteerLeft));
    CHECK(frame.held.count() == 2);
}

TEST_CASE("an action stays held while another of its inputs is still down") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;

    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_UP), latch);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 30000), latch);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 100), latch);
    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_SOUTH), latch);
    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_UP, SDL_GAMEPAD_BUTTON_SOUTH), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_W), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_UP, SDLK_W), latch);
    const sim::InputFrame held = latch.TakeFrame();

    CHECK(held.IsHeld(sim::Action::Accelerate));
    CHECK_FALSE(held.WasReleased(sim::Action::Accelerate));

    bindings.Handle(Key(SDL_EVENT_KEY_UP, SDLK_UP), latch);
    CHECK(latch.TakeFrame().WasReleased(sim::Action::Accelerate));
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

    CHECK_FALSE(frame.IsHeld(sim::Action::Accelerate));
    CHECK_FALSE(frame.IsHeld(sim::Action::Brake));
}

TEST_CASE("keys come from the settings and unknown names are skipped") {
    settings::Registry registry = DefaultRegistry();
    registry.Set(kActionBindings[0].keys, "Nonsense Key, J");
    InputBindings bindings(registry);
    sim::InputLatch latch;

    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_UP), latch);
    CHECK_FALSE(latch.TakeFrame().IsHeld(kActionBindings[0].action));
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_J), latch);
    CHECK(latch.TakeFrame().IsHeld(kActionBindings[0].action));
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

    CHECK(frame.IsHeld(sim::Action::Drift));
    CHECK(frame.IsHeld(sim::Action::Accelerate));
    CHECK(frame.WasPressed(sim::Action::Brake));
    CHECK_FALSE(frame.IsHeld(sim::Action::Brake));
}

TEST_CASE("the left stick steers left as positive with a dead zone") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;

    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_LEFTX, -32768), latch);
    CHECK(latch.TakeFrame().steerLeftAxis == 127);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_LEFTX, 16384), latch);
    CHECK(latch.TakeFrame().steerLeftAxis == -64);
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_LEFTX, 4000), latch);
    CHECK(latch.TakeFrame().steerLeftAxis == 0);
}

TEST_CASE("disconnecting a gamepad centres the stick") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;
    bindings.Handle(Axis(SDL_GAMEPAD_AXIS_LEFTX, -32768), latch);
    SDL_Event removed{};
    removed.type = SDL_EVENT_GAMEPAD_REMOVED;

    bindings.Handle(removed, latch);

    CHECK(latch.TakeFrame().steerLeftAxis == 0);
}

TEST_CASE("menu keys press on every repeat and gamepad buttons press menu actions") {
    InputBindings bindings(DefaultRegistry());
    sim::InputLatch latch;

    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_DOWN), latch);
    CHECK(latch.TakeFrame().WasPressed(sim::MenuAction::Down));
    bindings.Handle(Key(SDL_EVENT_KEY_DOWN, SDLK_DOWN, true), latch);
    const sim::InputFrame repeat = latch.TakeFrame();
    CHECK(repeat.WasPressed(sim::MenuAction::Down));
    CHECK_FALSE(repeat.WasPressed(sim::Action::Brake));

    bindings.Handle(Button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_SOUTH), latch);
    bindings.Handle(Key(SDL_EVENT_KEY_UP, SDLK_RETURN), latch);
    const sim::InputFrame frame = latch.TakeFrame();
    CHECK(frame.WasPressed(sim::MenuAction::Confirm));
    CHECK(frame.menuPressed.count() == 1);
}

}
}
