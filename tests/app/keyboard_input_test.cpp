#include "app/keyboard_input.hpp"

#include <doctest/doctest.h>

namespace rr::app {
namespace {

SDL_Event Key(SDL_EventType type, SDL_Keycode key, bool isRepeat = false) {
    SDL_Event event{};
    event.type = type;
    event.key.key = key;
    event.key.repeat = isRepeat;
    return event;
}

TEST_CASE("arrow keys drive and repeats are ignored") {
    sim::InputLatch latch;

    LatchKey(Key(SDL_EVENT_KEY_DOWN, SDLK_UP), latch);
    LatchKey(Key(SDL_EVENT_KEY_DOWN, SDLK_LEFT), latch);
    LatchKey(Key(SDL_EVENT_KEY_UP, SDLK_LEFT), latch);
    LatchKey(Key(SDL_EVENT_KEY_DOWN, SDLK_UP, true), latch);
    LatchKey(Key(SDL_EVENT_KEY_DOWN, SDLK_F1), latch);
    const sim::InputFrame frame = latch.TakeFrame();

    CHECK(frame.IsHeld(sim::Action::Accelerate));
    CHECK(frame.WasPressed(sim::Action::SteerLeft));
    CHECK(frame.WasReleased(sim::Action::SteerLeft));
    CHECK_FALSE(frame.IsHeld(sim::Action::SteerLeft));
    CHECK(frame.held.count() == 1);
}

}
}
