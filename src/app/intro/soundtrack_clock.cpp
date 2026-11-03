#include "app/intro/soundtrack_clock.hpp"

#include <chrono>

namespace rr::app {

std::optional<Microseconds> SoundtrackClock::Now(Microseconds simElapsed,
                                                 std::optional<Milliseconds> played) {
    if (hasStalled_) {
        return simElapsed;
    }
    if (!played) {
        return std::nullopt;
    }
    if (played != lastPlayed_) {
        lastPlayed_ = played;
        lastAdvance_ = simElapsed;
    } else if (simElapsed - lastAdvance_ > kSoundtrackStall) {
        hasStalled_ = true;
        return simElapsed;
    }
    return std::chrono::duration_cast<Microseconds>(*played);
}

}
