#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>

#include <vector>

namespace rr::platform {

/// Main thread only.
class EventPump {
public:
    void Poll();

    [[nodiscard]] bool IsQuitRequested() const;

    /// Covers the last `Poll` only. Key repeats don't count.
    [[nodiscard]] bool WasKeyPressed(SDL_Keycode key) const;

private:
    void Handle(const SDL_Event& event);

    bool isQuitRequested_ = false;
    std::vector<SDL_Keycode> pressedKeys_;
};

}
