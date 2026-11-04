#pragma once

#include <cstddef>
#include <cstdint>

#include "assets/race_route.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"

namespace rr::sim {

class RoutePlayback {
public:
    explicit RoutePlayback(assets::RaceRoute route);

    /// Runs backward when `speed` is negative, stopping at the route's start.
    void Advance(Milliseconds elapsed, float speed = 1.0F);

    /// In game space.
    [[nodiscard]] Transform Pose() const { return PoseAt(cursor_); }

    /// The pose `routeMs` of route time ahead, in game space.
    [[nodiscard]] Transform PoseAhead(float routeMs) const;

    /// Ignores the part of `move` off the current segment, and stops at the route's start.
    void Seek(const Vector3& move);

    /// How far the car may stray left and right of the line at the point being approached.
    [[nodiscard]] float WidthLeft() const { return route_.points[cursor_.target].widthLeft; }
    [[nodiscard]] float WidthRight() const { return route_.points[cursor_.target].widthRight; }

    /// The index of the route point being approached.
    [[nodiscard]] std::size_t Target() const { return cursor_.target; }

    /// How far along the segment to `Target()`, from 0 to 1.
    [[nodiscard]] float Alpha() const { return AlphaAt(cursor_); }

    /// Wheels on the ground at the point being approached: 0, 1, 2 or 4.
    [[nodiscard]] int ContactCount() const { return cursor_.contactCount; }

    void Save() { saved_ = cursor_; }
    void Restore() { cursor_ = saved_; }

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
    void MoveBack(Cursor& cursor) const;
    void StartSegment(Cursor& cursor, std::size_t target, Transform from,
                      std::int32_t startMs) const;
    [[nodiscard]] std::int32_t SegmentEndMs(const Cursor& cursor) const;
    [[nodiscard]] Transform PoseAt(const Cursor& cursor) const;
    [[nodiscard]] float AlphaAt(const Cursor& cursor) const;

    assets::RaceRoute route_;
    Cursor cursor_;
    Cursor saved_;
};

}
