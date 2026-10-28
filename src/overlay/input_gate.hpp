#pragma once

#include <SDL3/SDL_events.h>

namespace rr::overlay {

/// Passes events to ImGui only while open. Opening drops queued and held input, so nothing done
/// while closed applies.
class InputGate {
public:
    void Forward(const SDL_Event& event) const;
    void SetOpen(bool isOpen);
    [[nodiscard]] bool IsOpen() const { return isOpen_; }

private:
    bool isOpen_ = false;
};

}
