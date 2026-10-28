#pragma once

#include <SDL3/SDL_events.h>

namespace rr::platform {

/// Main thread only.
class EventPump {
public:
    void Poll();

    [[nodiscard]] bool IsQuitRequested() const;

private:
    void Handle(const SDL_Event& event);

    bool isQuitRequested_ = false;
};

}
