#pragma once

#include <cstddef>

#include "assets/race_route.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"

namespace rr::sim {

class RoutePlayback {
public:
    explicit RoutePlayback(assets::RaceRoute route);

    void Advance(Milliseconds elapsed);

    /// In game space.
    [[nodiscard]] Transform Pose() const;

private:
    void StartSegment(std::size_t target, Transform from);

    assets::RaceRoute route_;
    std::size_t target_ = 0;
    Transform from_;
    Transform to_;
    Milliseconds intoSegment_{};
};

}
