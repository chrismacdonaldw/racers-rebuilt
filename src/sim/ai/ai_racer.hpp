#pragma once

#include "assets/race_route.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "sim/route_playback.hpp"

namespace rr::sim {

/// Replays a route from its start pose instead of driving; only its playback speed changes.
class AiRacer {
public:
    explicit AiRacer(assets::RaceRoute route);

    void Tick(Milliseconds elapsed);

    /// The speed playback eases toward, 1 for the route as recorded.
    void SetTargetSpeed(float speed) { targetSpeed_ = speed; }

    [[nodiscard]] float PlaybackSpeed() const { return playbackSpeed_; }

    /// In game space.
    [[nodiscard]] Transform Pose() const { return route_.Pose(); }

    /// The car's forward axis in game space.
    [[nodiscard]] Vector3 Facing() const;

    /// In units per ms over the last tick.
    [[nodiscard]] Vector3 Velocity() const { return velocity_; }

    [[nodiscard]] int ContactCount() const { return route_.ContactCount(); }

private:
    void EaseTowardTarget(float elapsedMs);

    RoutePlayback route_;
    float playbackSpeed_ = 1.0F;
    float targetSpeed_ = 1.0F;
    Vector3 velocity_;
};

}
