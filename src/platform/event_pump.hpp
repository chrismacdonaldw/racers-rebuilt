#pragma once

#include <SDL3/SDL_events.h>

namespace rr::platform {

/// Main thread only.
class EventPump {
public:
    /// If waiting fails, logs the error and requests quit.
    void Wait();

    [[nodiscard]] bool IsQuitRequested() const;

private:
    void Handle(const SDL_Event& event);

    bool isQuitRequested_ = false;
};

}
