#pragma once

#include "assets/race_route.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "sim/ai/route_effects.hpp"
#include "sim/route_playback.hpp"

namespace rr::sim {

/// Replays a route from its start pose instead of driving; only its playback speed changes.
class AiRacer {
public:
    explicit AiRacer(assets::RaceRoute route, const Vector3& wheelAnchor = {});

    void Tick(Milliseconds elapsed);

    /// The speed playback eases toward, 1 for the route as recorded.
    void SetTargetSpeed(float speed) { targetSpeed_ = speed; }

    /// The pacing speed, which the target follows unless a push holds it.
    void SetBaseSpeed(float speed);

    /// `ratePerMs` is in radians per ms. Ignored while already spinning.
    void StartSpin(float turns, float ratePerMs);
    void EndSpin();
    void StartSpinOut();
    void StartHover();
    void StopHover();

    /// `ratePerMs` is in radians per ms. Ignored while a tilt is already running.
    void Tilt(float ratePerMs, int durationMs);

    void StartPush(const Vector3& force);
    void EndPush();
    void StartBoost();
    void EndBoost();
    void StartCurseSlow();
    void EndCurseSlow();
    void StartGhost();
    void EndGhost();
    void HoldAt(float speed);
    void Release();

    /// Takes a contact's push as a seek along the route and a side offset within its width.
    void Shift(const Vector3& move);

    /// Takes a contact's impulse of `impulse` along `direction` as a change of playback speed.
    void Bump(const Vector3& direction, float impulse);

    [[nodiscard]] float PlaybackSpeed() const { return playbackSpeed_; }
    [[nodiscard]] float SideOffset() const { return sideOffset_; }
    [[nodiscard]] float TargetSpeed() const { return targetSpeed_; }
    [[nodiscard]] const RouteEffects& Effects() const { return effects_; }

    /// In game space.
    [[nodiscard]] Transform Pose() const { return pose_; }

    /// The car's forward axis in game space.
    [[nodiscard]] Vector3 Facing() const;

    /// In units per ms over the last tick.
    [[nodiscard]] Vector3 Velocity() const { return velocity_; }

    [[nodiscard]] int ContactCount() const { return route_.ContactCount(); }

private:
    void EaseTowardTarget(float elapsedMs);
    void AimBank();
    [[nodiscard]] bool HoldsSpeed() const;
    void ReturnToFullSpeed();
    void Place(int dtMs);

    RoutePlayback route_;
    Vector3 wheelAnchor_;
    Transform pose_;
    float playbackSpeed_ = 1.0F;
    float targetSpeed_ = 1.0F;
    float baseSpeed_ = 1.0F;
    float sideOffset_ = 0.0F;
    Vector3 velocity_;
    RouteEffects effects_;
};

}
