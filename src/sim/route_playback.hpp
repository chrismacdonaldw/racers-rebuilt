#pragma once

#include <cstddef>
#include <cstdint>

#include "assets/race_route.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"

namespace rr::sim {

class RoutePlayback {
public:
    explicit RoutePlayback(assets::RaceRoute route);

    /// Moves route time on by `speed` times `elapsed`. `speed` must not be negative.
    void Advance(Milliseconds elapsed, float speed = 1.0F);

    /// In game space.
    [[nodiscard]] Transform Pose() const;

    /// Wheels on the ground at the point being approached: 0, 1, 2 or 4.
    [[nodiscard]] int ContactCount() const { return contactCount_; }

private:
    void StartSegment(std::size_t target, Transform from, std::int32_t startMs);
    [[nodiscard]] std::int32_t SegmentEndMs() const;

    assets::RaceRoute route_;
    std::size_t target_ = 0;
    Transform from_;
    Transform to_;
    std::int32_t segmentStartMs_ = 0;
    float timeMs_ = 0.0F;
    int contactCount_ = 0;
};

}
