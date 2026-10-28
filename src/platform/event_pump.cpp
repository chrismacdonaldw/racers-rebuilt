#include "platform/event_pump.hpp"

#include <SDL3/SDL_error.h>

#include "core/log.hpp"

namespace rr::platform {

void EventPump::Wait() {
    SDL_Event event;
    if (!SDL_WaitEvent(&event)) {
        log::Error("waiting for events failed, quitting: {}", SDL_GetError());
        isQuitRequested_ = true;
        return;
    }

    Handle(event);
    while (SDL_PollEvent(&event)) {
        Handle(event);
    }
}

bool EventPump::IsQuitRequested() const {
    return isQuitRequested_;
}

void EventPump::Handle(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            isQuitRequested_ = true;
            break;
        default:
            break;
    }
}

}
