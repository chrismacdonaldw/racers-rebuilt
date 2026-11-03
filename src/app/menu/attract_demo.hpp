#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

#include "assets/race_catalog.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"

namespace rr::app {

inline constexpr Milliseconds kAttractTimeout{60000};

class AttractTimer {
public:
    /// While `canLeave` is false, as with a dialog open, a finished timer waits at zero.
    [[nodiscard]] bool Tick(Milliseconds elapsed, bool canLeave) {
        left_ -= std::min(elapsed, left_);
        return left_ == Milliseconds{0} && canLeave;
    }

private:
    Milliseconds left_ = kAttractTimeout;
};

/// A computer-only race of circuit `c0`'s race in position `roll` modulo the circuit's races.
[[nodiscard]] Result<std::string> AttractDemoScene(const assets::RaceCatalog& races,
                                                   std::uint32_t roll);

}
