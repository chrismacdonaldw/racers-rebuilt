#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "assets/race_route.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "sim/ai/ai_racer.hpp"
#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/curse_jitter.hpp"
#include "sim/physics/driver_input.hpp"
#include "sim/physics/racer_contacts.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/physics/start_boost.hpp"
#include "sim/physics/turbo.hpp"
#include "sim/race/random.hpp"

namespace rr::sim {

struct RacerStart {
    Vector3 position;
    float headingRadians = 0.0F;
};

/// What happened in the racer's last `Tick`, and in any `Go` or `StartTurbo` call since.
struct RacerEvents {
    std::optional<TurboLevel> turboStarted;
    std::optional<TurboLevel> turboFaded;
    std::optional<std::uint8_t> scrapeSound;
    bool hasDriftStarted = false;
    bool hasWarpEntered = false;
    bool hasWarpLanded = false;
};

struct ShieldStatus {
    std::optional<std::uint8_t> level;
    std::uint32_t raised = 0;
    std::uint32_t absorbs = 0;
    std::uint8_t absorbSound = 0;
    int cooldownMs = 0;
};

struct ContactStatus {
    bool isTouching = false;
    int boxMs = 0;
    float boxScale = 1.0F;
    int scrapeCooldownMs = 0;
};

struct RacerStatus {
    bool isHalted = false;
    bool isMagnetHeld = false;
    bool isGhost = false;
    bool isWarping = false;
    bool isCursed = false;
    int curseMs = 0;
    CurseJitter jitter;
    std::uint32_t hitReactions = 0;
    ShieldStatus shield;
    std::optional<std::size_t> shoveForce;
    int shoveMs = 0;
    ContactStatus contact;
};

class Racer {
public:
    Racer(const ChassisSpec& chassis, const CarBuild& build, const RacerStart& start);

    /// `elapsed` is a whole number of ms, at most the original's 40 ms substep.
    void Tick(Milliseconds elapsed, const RacerControls& controls, const CollisionQuery& world);

    /// Pedals and drift do nothing until `Go`.
    void HoldAtStart();
    void Go();

    /// Replaces any turbo already running. A hover turbo halts the racer until it ends.
    void StartTurbo(TurboLevel level);
    void CancelTurbo();

    /// Counts every `StartTurbo`, so an owner can tell its turbo from a later one.
    [[nodiscard]] std::uint32_t TurboCount() const { return turboCount_; }

    /// Puts the racer at rest at `start`, as if it were built there.
    void Place(const RacerStart& start);

    /// `ratePerMs` is in radians per ms. `align` lets a brake stop the spin after that many turns.
    void StartSpin(float turns, float ratePerMs, float align = 0.0F);
    void StartSpinOut();

    /// Spins out, and off a route throws the car forward and up. `strength` runs from 0 to 1.
    void LaunchHit(float strength);

    /// `ratePerMs` is in radians per ms. Ignored while a tilt is already running.
    void KickPitch(float ratePerMs, Milliseconds duration);

    /// Off a route, adds `impulse` over the car's mass along `direction`; on one, bumps playback.
    void Push(const Vector3& direction, float impulse);

    void Halt();
    void Resume();
    void AttachCurse(Milliseconds duration);
    void RemoveCurse();

    /// `force` is in units per s² for the base mass.
    void StartExternalForce(std::size_t slot, const Vector3& force);
    void EndExternalForce(std::size_t slot);

    void StartMagnetHold();
    void EndMagnetHold();
    void EnterGhostMode();
    void LeaveGhostMode();
    void ReactToHit();

    /// Fails while the racer is a ghost or already warping. Ends any magnet hold.
    [[nodiscard]] bool BeginWarp();
    void CancelWarp();
    void EnterWarp();

    /// Does nothing to a racer on a route, which keeps playing it.
    void MoveWarp(const Vector3& position);

    /// Off a route, sets the car down at `position` and launches it along `direction`.
    void LandWarp(const Vector3& position, const Vector3& direction);

    /// Replaces any shield already up, and lifts a curse.
    void RaiseShield(std::uint8_t level);
    void LowerShield();

    /// Rolls the absorb sound from `random`, unless an absorb in the last 500 ms already did.
    void AbsorbShieldHit(Random& random);

    /// Pushes with `force` for 750 ms. Does nothing while a shove runs or no force slot is free.
    void Shove(const Vector3& force);

    /// Starts shrinking the contact box over 750 ms, unless it is already shrinking.
    void ShrinkBox();
    void MarkTouching();

    [[nodiscard]] bool IsScrapeReady() const { return status_.contact.scrapeCooldownMs == 0; }
    void Scrape(std::uint8_t sound);
    void HoldScrape();

    /// Runs before each `Tick`, drawing from `random` while cursed.
    void RollCurse(Milliseconds elapsed, Random& random);

    /// Runs once per race tick, after every racer's `Tick`.
    void UpdateTimers(Milliseconds elapsed);

    [[nodiscard]] bool IsSpinning() const;

    /// Off a route this lasts only until `UpdateTimers`.
    [[nodiscard]] bool IsSpunOut() const;
    [[nodiscard]] bool IsShielded() const { return status_.shield.level.has_value(); }
    [[nodiscard]] const RacerStatus& Status() const { return status_; }

    /// From now on the racer replays `route` from its start, ignoring controls and the world.
    void FollowRoute(assets::RaceRoute route);

    /// Does nothing unless the racer follows a route.
    void SetRouteSpeed(float speed);

    [[nodiscard]] const std::optional<AiRacer>& Route() const { return route_; }

    /// In game space.
    [[nodiscard]] Transform Pose() const;

    [[nodiscard]] const BodyState& Body() const { return body_; }

    [[nodiscard]] Vector3 CentreOfMass() const;

    [[nodiscard]] const DriverState& Driver() const { return driver_.State(); }

    [[nodiscard]] const StatMultipliers& Stats() const { return stats_; }

    [[nodiscard]] const StartBoostState& Boost() const { return startBoost_.State(); }

    [[nodiscard]] const std::optional<Turbo>& ActiveTurbo() const { return turbo_; }

    [[nodiscard]] const RacerEvents& Events() const { return events_; }

    /// `world` is the one this racer stepped against.
    [[nodiscard]] ContactBody ForContacts(const CollisionQuery& world);

private:
    void UpdateTurbo(int dtMs);
    void FinishTurbo();
    [[nodiscard]] bool HasHoverStalled() const;
    void KeepInWorldBounds();
    void UpdateContactTimers(int elapsedMs);
    void TakeRoutePose(const AiRacer& route);
    [[nodiscard]] DriveCommand AffectedDrive(DriveCommand command);

    BodyConstants constants_;
    StatMultipliers stats_;
    BodyState body_;
    DriverInput driver_;
    StartBoost startBoost_;
    std::optional<Turbo> turbo_;
    std::optional<AiRacer> route_;
    RacerEvents events_;
    RacerStatus status_;
    bool wasBrakeHeld_ = false;
    std::uint32_t turboCount_ = 0;
};

}
