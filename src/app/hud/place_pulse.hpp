#pragma once

#include <optional>

#include "core/duration.hpp"

namespace rr::app {

/// Holds each newly shown place for 1050 ms before taking a newer one. Starts on 9.
class PlacePulse {
public:
    void Advance(int place, Milliseconds elapsed);

    [[nodiscard]] int Shown() const { return shown_; }
    [[nodiscard]] float Scale() const;

private:
    int shown_ = 9;
    std::optional<Milliseconds> sinceChange_;
};

}
