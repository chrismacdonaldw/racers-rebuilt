#include "app/keyboard_input.hpp"

#include <optional>

namespace rr::app {
namespace {

std::optional<sim::Action> ActionFor(SDL_Keycode key) {
    switch (key) {
        case SDLK_UP:
        case SDLK_W:
            return sim::Action::Accelerate;
        case SDLK_DOWN:
        case SDLK_S:
            return sim::Action::Brake;
        case SDLK_LEFT:
        case SDLK_A:
            return sim::Action::SteerLeft;
        case SDLK_RIGHT:
        case SDLK_D:
            return sim::Action::SteerRight;
        case SDLK_SPACE:
            return sim::Action::UsePowerup;
        case SDLK_ESCAPE:
            return sim::Action::Pause;
        default:
            return std::nullopt;
    }
}

}

void LatchKey(const SDL_Event& event, sim::InputLatch& latch) {
    const bool isDown = event.type == SDL_EVENT_KEY_DOWN;
    if ((!isDown && event.type != SDL_EVENT_KEY_UP) || event.key.repeat) {
        return;
    }
    if (const std::optional<sim::Action> action = ActionFor(event.key.key)) {
        if (isDown) {
            latch.Press(*action);
        } else {
            latch.Release(*action);
        }
    }
}

}
