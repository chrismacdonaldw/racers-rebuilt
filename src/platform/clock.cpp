#include "platform/clock.hpp"

#include <SDL3/SDL_timer.h>

#include <cstdint>

namespace rr::platform {

Nanoseconds Now() {
    return Nanoseconds{static_cast<std::int64_t>(SDL_GetTicksNS())};
}

}
