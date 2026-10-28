#include "platform/event_pump.hpp"

namespace rr::platform {

void EventPump::Poll() {
    SDL_Event event;
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
