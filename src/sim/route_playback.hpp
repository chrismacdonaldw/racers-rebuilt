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
    [[nodiscard]] Transform Pose() const { return PoseAt(cursor_); }

    /// The pose `routeMs` of route time ahead, in game space.
    [[nodiscard]] Transform PoseAhead(float routeMs) const;

    /// Wheels on the ground at the point being approached: 0, 1, 2 or 4.
    [[nodiscard]] int ContactCount() const { return cursor_.contactCount; }

private:
    struct Cursor {
        std::size_t target = 0;
        Transform from;
        Transform to;
        std::int32_t segmentStartMs = 0;
        float timeMs = 0.0F;
        int contactCount = 0;
    };

    void Move(Cursor& cursor, float routeMs) const;
    void StartSegment(Cursor& cursor, std::size_t target, Transform from,
                      std::int32_t startMs) const;
    [[nodiscard]] std::int32_t SegmentEndMs(const Cursor& cursor) const;
    [[nodiscard]] Transform PoseAt(const Cursor& cursor) const;

    assets::RaceRoute route_;
    Cursor cursor_;
};

}
