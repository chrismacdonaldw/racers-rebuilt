#pragma once

#include <SDL3/SDL_events.h>

#include "sim/input_latch.hpp"

namespace rr::app {

/// Arrows or WASD drive, Space uses the powerup and Escape pauses.
void LatchKey(const SDL_Event& event, sim::InputLatch& latch);

}
