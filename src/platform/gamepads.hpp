#pragma once

#include <SDL3/SDL_events.h>

#include <memory>
#include <vector>

struct SDL_Gamepad;

namespace rr::platform {

/// Requires a live `SdlContext`. Without a gamepad subsystem it logs once and opens nothing.
class Gamepads {
public:
    Gamepads();
    Gamepads(const Gamepads&) = delete;
    Gamepads(Gamepads&&) = delete;
    Gamepads& operator=(const Gamepads&) = delete;
    Gamepads& operator=(Gamepads&&) = delete;
    ~Gamepads();

    void Handle(const SDL_Event& event);

private:
    struct Deleter {
        void operator()(SDL_Gamepad* gamepad) const;
    };

    bool isStarted_ = false;
    std::vector<std::unique_ptr<SDL_Gamepad, Deleter>> open_;
};

}
