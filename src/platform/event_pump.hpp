#pragma once

#include <SDL3/SDL_events.h>

namespace rr::platform {

/// Drains SDL's event queue on the main thread and records quit requests.
class EventPump {
public:
    /// Blocks until at least one event arrives, then handles every pending event.
    ///
    /// If waiting fails, logs the error and requests quit.
    void Wait();

    /// Returns true once the window has been closed or the system has asked the app to quit.
    [[nodiscard]] bool IsQuitRequested() const;

private:
    void Handle(const SDL_Event& event);

    bool isQuitRequested_ = false;
};

}
