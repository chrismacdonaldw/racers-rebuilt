#pragma once

#include <optional>

#include "core/duration.hpp"

namespace rr::app {

inline constexpr Milliseconds kSoundtrackStall{500};

/// Soundtrack time while it advances, then sim time for good once it stalls `kSoundtrackStall`.
class SoundtrackClock {
public:
    /// `played` is nothing once the soundtrack has ended, and then so is the result.
    [[nodiscard]] std::optional<Microseconds> Now(Microseconds simElapsed,
                                                  std::optional<Milliseconds> played);

private:
    std::optional<Milliseconds> lastPlayed_;
    Microseconds lastAdvance_{0};
    bool hasStalled_ = false;
};

}
