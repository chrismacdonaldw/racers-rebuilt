#include "platform/gamepads.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_init.h>

#include "core/log.hpp"

namespace rr::platform {

void Gamepads::Deleter::operator()(SDL_Gamepad* gamepad) const {
    SDL_CloseGamepad(gamepad);
}

Gamepads::Gamepads() : isStarted_(SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
    if (!isStarted_) {
        log::Warn("gamepads unavailable, keyboard only: {}", SDL_GetError());
    }
}

Gamepads::~Gamepads() {
    open_.clear();
    if (isStarted_) {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }
}

void Gamepads::Handle(const SDL_Event& event) {
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (SDL_Gamepad* gamepad = SDL_OpenGamepad(event.gdevice.which)) {
            const char* name = SDL_GetGamepadName(gamepad);
            log::Info("gamepad connected: {}", name != nullptr ? name : "unnamed");
            open_.emplace_back(gamepad);
        }
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        std::erase_if(open_, [&event](const auto& gamepad) {
            return SDL_GetGamepadID(gamepad.get()) == event.gdevice.which;
        });
    }
}

}
