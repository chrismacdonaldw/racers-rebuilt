#pragma once

#include <optional>

#include "core/duration.hpp"

namespace rr::app {

/// Swells the place digit to twice its size and back over 350 ms whenever the place changes.
class PlacePulse {
public:
    void Advance(int place, Milliseconds elapsed);

    [[nodiscard]] float Scale() const;

private:
    std::optional<int> place_;
    std::optional<Milliseconds> sinceChange_;
};

}
