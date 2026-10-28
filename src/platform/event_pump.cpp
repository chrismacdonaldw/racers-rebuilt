#include "platform/event_pump.hpp"

#include <algorithm>
#include <utility>

namespace rr::platform {

void EventPump::Poll() {
    pressedKeys_.clear();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        for (const Observer& observer : observers_) {
            observer(event);
        }
        Handle(event);
    }
}

void EventPump::Observe(Observer observer) {
    observers_.push_back(std::move(observer));
}

bool EventPump::IsQuitRequested() const {
    return isQuitRequested_;
}

bool EventPump::WasKeyPressed(SDL_Keycode key) const {
    return std::ranges::contains(pressedKeys_, key);
}

void EventPump::Handle(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            isQuitRequested_ = true;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (!event.key.repeat) {
                pressedKeys_.push_back(event.key.key);
            }
            break;
        default:
            break;
    }
}

}
