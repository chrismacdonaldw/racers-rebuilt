#include "sim/replay/snapshot_hash.hpp"

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>
#include <type_traits>
#include <variant>

#include "sim/replay/replay_bytes.hpp"

namespace rr::sim {
namespace {

constexpr std::string_view kMagic = "RRRH";
constexpr std::uint16_t kVersion = 1;
constexpr std::size_t kHashBytes = 8;
constexpr std::size_t kNoWhiteBrick = std::numeric_limits<std::size_t>::max();
constexpr std::uint8_t kNoOwner = std::numeric_limits<std::uint8_t>::max();

template <std::unsigned_integral T>
std::uint64_t Mix(std::uint64_t hash, T value) {
    std::array<std::byte, sizeof(T)> bytes{};
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        bytes.at(i) = static_cast<std::byte>((value >> (8U * i)) & 0xFFU);
    }
    return HashBytes(bytes, hash);
}

std::uint64_t MixFloats(std::uint64_t hash, std::initializer_list<float> values) {
    for (const float value : values) {
        hash = Mix(hash, std::bit_cast<std::uint32_t>(value));
    }
    return hash;
}

std::uint64_t MixTransform(std::uint64_t hash, const Transform& transform) {
    return MixFloats(hash, {transform.position.x, transform.position.y, transform.position.z,
                            transform.rotation.w, transform.rotation.x, transform.rotation.y,
                            transform.rotation.z});
}

class StateHash {
public:
    [[nodiscard]] std::uint64_t Value() const { return value_; }

    void Add(float value) { value_ = Mix(value_, std::bit_cast<std::uint32_t>(value)); }
    void Add(int value) { value_ = Mix(value_, static_cast<std::uint32_t>(value)); }
    void Add(bool value) { value_ = Mix(value_, static_cast<std::uint8_t>(value ? 1U : 0U)); }
    void Add(std::uint64_t value) { value_ = Mix(value_, value); }
    void Add(Milliseconds value) {
        value_ = Mix(value_, static_cast<std::uint64_t>(value.count()));
    }
    void Add(const Vector3& v) { Add(v.x, v.y, v.z); }
    void Add(const Transform& transform) { value_ = MixTransform(value_, transform); }
    void Add(const Basis& basis) { Add(basis.forward, basis.side, basis.up); }

    void Add(const Surface& surface) {
        Add(surface.rollingResistance, surface.supportThreshold, surface.friction,
            surface.lateralGrip, surface.force);
    }

    void Add(const WheelProbe& wheel) {
        Add(wheel.position, wheel.hasHit, wheel.hitPoint, wheel.hitNormal, wheel.hitHeight,
            wheel.isInContact, wheel.contactForce, wheel.hitSurface, wheel.surface);
    }

    void Add(const BodyMotion& motion) {
        Add(motion.position, motion.basis, motion.velocity, motion.angularMomentum);
        for (const WheelProbe& wheel : motion.wheels) {
            Add(wheel);
        }
        Add(motion.contactCount, motion.supportNormal);
    }

    void Add(const DerivedMotion& derived) {
        Add(derived.speed, derived.velocityDirection, derived.forwardSpeed, derived.forwardVelocity,
            derived.lateralVelocity, derived.isAtRest);
    }

    void Add(const SteeringState& steering) {
        Add(steering.isSteering, steering.gain, steering.slip, steering.alignmentMinimum,
            steering.lagMaximum);
    }

    void Add(const ImpulseTimers& timers) { Add(timers.yawMs, timers.pitchMs, timers.rollMs); }

    void Add(const SpinState& spin) {
        Add(spin.isSpinning, spin.isFresh, spin.periodMs, spin.stopAtMs);
    }

    void Add(const HoverLift& lift) {
        Add(lift.height, lift.targetHeight, lift.ratePerSecond, lift.bank, lift.targetBank);
    }

    void Add(const HoverState& hover) {
        Add(hover.isHovering, hover.lift, hover.frontAxle, hover.rearAxle);
    }

    void Add(const std::optional<Vector3>& force) {
        Add(force.has_value(), force.value_or(Vector3{}));
    }

    void Add(const BodyState& body) {
        Add(body.motion, body.saved, body.angularVelocity, body.force, body.torque, body.facing,
            body.derived, body.turnRadius, body.yawRate, body.thrust, body.maxSpeed,
            body.dragCoefficient, body.gripScale, body.steering, body.impulses, body.hasWallYaw,
            body.hasWallContact, body.airborneMs, body.supportForce, body.spin, body.isSpunOut,
            body.spinOutMs, body.hover, body.externalForces[0], body.externalForces[1],
            body.hasTrackCollision);
    }

    void Add(const ShieldStatus& shield) {
        Add(shield.level.has_value(), static_cast<int>(shield.level.value_or(0)),
            static_cast<std::uint64_t>(shield.raised), static_cast<std::uint64_t>(shield.absorbs),
            static_cast<int>(shield.absorbSound), shield.cooldownMs);
    }

    void Add(const ContactStatus& contact) {
        Add(contact.isTouching, contact.boxMs, contact.boxScale, contact.scrapeCooldownMs);
    }

    void Add(const RacerStatus& status) {
        Add(status.isHalted, status.isMagnetHeld, status.isGhost, status.isWarping, status.isCursed,
            status.curseMs, status.jitter.countdownMs, status.jitter.throttleOffset,
            status.jitter.steerOffset, static_cast<std::uint64_t>(status.hitReactions),
            status.shield, status.shoveForce.has_value(),
            static_cast<std::uint64_t>(status.shoveForce.value_or(0)), status.shoveMs,
            status.contact);
    }

    void Add(const RouteEffects& effects) {
        Add(effects.isSpinning, effects.isSpunOut, effects.isHovering, effects.isBoosting,
            effects.isGhost, effects.isCurseSlowed, effects.isPushed);
        Add(effects.spin.remainingMs, effects.spin.ratePerMs, effects.spin.angle);
        Add(effects.tilt.remainingMs, effects.tilt.ratePerMs, effects.tilt.angle,
            effects.tilt.height);
        Add(effects.hop.velocity, effects.hop.height, effects.lift);
    }

    void Add(const DriverState& driver) {
        Add(driver.steering, driver.isDrifting, driver.isHardDrift);
    }

    void Add(const StartBoostState& boost) {
        Add(boost.isHeldAtStart, boost.windowMs, boost.heldMs, boost.isThrottleHeld);
    }

    void Add(const std::optional<Turbo>& turbo) {
        Add(turbo.has_value());
        if (turbo) {
            Add(static_cast<int>(turbo->level), static_cast<int>(turbo->phase), turbo->phaseMs,
                turbo->hasTouchedWall);
        }
    }

    void Add(const std::optional<AiRacer>& route) {
        Add(route.has_value());
        if (route) {
            Add(route->PlaybackSpeed(), route->TargetSpeed(), route->SideOffset(),
                route->Velocity(), route->ContactCount(), route->Effects());
        }
    }

    void Add(const RoutePlayback& playback) { Add(playback.Pose(), playback.ContactCount()); }

    void Add(const std::optional<RouteDriver>& driver) {
        Add(driver.has_value());
        if (driver) {
            Add(driver->Nearest(), driver->Reference(), driver->Ahead(), driver->ReferenceLead(),
                driver->AheadLead(), driver->StalledFor(), driver->HasHeadway());
        }
    }

    void Add(const LapTracker& laps) {
        Add(laps.CompletedLaps(), laps.IsWrongWay(), laps.LastCheckpoint().has_value());
        Add(static_cast<std::uint64_t>(laps.LastCheckpoint().value_or(0)));
    }

    void Add(const RacerProgress& progress) {
        Add(progress.laps, static_cast<std::uint64_t>(progress.lapTimes.size()),
            progress.finishPlace.value_or(0));
        for (const Milliseconds lap : progress.lapTimes) {
            Add(lap);
        }
    }

    void Add(const Brick& brick) {
        Add(brick.position, static_cast<int>(brick.state), brick.stateTime, brick.isTouched,
            brick.wasTouched);
    }

    void Add(const ColourBrick& colour) {
        Add(colour.brick, static_cast<int>(colour.colour), static_cast<int>(colour.nextColour),
            static_cast<int>(colour.nextState));
    }

    void Add(const WhiteBrick& white) {
        Add(white.brick, white.droppedTime.value_or(Milliseconds{-1}), white.isGoingHome);
    }

    void Add(const BrickField& bricks) {
        Add(bricks.SpinRadians());
        for (const ColourBrick& colour : bricks.Coloured()) {
            Add(colour);
        }
        for (const WhiteBrick& white : bricks.White()) {
            Add(white);
        }
    }

    void Add(const PowerupInventory& inventory) {
        Add(inventory.Colour().has_value(),
            static_cast<int>(inventory.Colour().value_or(assets::BrickColour::Red)));
        for (const std::optional<std::size_t> white : inventory.WhiteBricks()) {
            Add(static_cast<std::uint64_t>(white.value_or(kNoWhiteBrick)));
        }
        Add(static_cast<std::uint64_t>(inventory.WhiteCount()));
    }

    void Add(const PowerupSystem& powerups) {
        for (const std::uint16_t slot : powerups.Active()) {
            const EffectSlot& effect = powerups.Slots()[slot];
            const EffectProgress progress = effect.effect->Progress();
            const Transform pose = effect.effect->Pose();
            Add(static_cast<int>(slot), static_cast<int>(effect.kind),
                static_cast<int>(effect.use.racer), static_cast<int>(effect.use.level),
                static_cast<int>(effect.use.shot), static_cast<int>(progress.phase),
                progress.phaseTime, pose.position, pose.rotation.w, pose.rotation.x,
                pose.rotation.y, pose.rotation.z, effect.entities.to_ullong());
            if (effect.use.hazard) {
                Add(static_cast<int>(*effect.use.hazard));
            }
        }
    }

    void Add(const Explosions& explosions) {
        for (const Explosion& explosion : explosions.All()) {
            if (explosion.phase == ExplosionPhase::Idle) {
                continue;
            }
            Add(explosion.position, static_cast<int>(explosion.owner.value_or(kNoOwner)),
                static_cast<int>(explosion.phase), explosion.remaining, explosion.growth,
                explosion.growthRate, explosion.leavesScar);
        }
    }

    void Add(const TrackEvents& triggers) {
        for (const TrackEvent& event : triggers.Events()) {
            Add(event.id, static_cast<int>(event.edge),
                static_cast<int>(event.racer.value_or(kNoOwner)));
        }
        for (std::size_t i = 0; i < triggers.RacerSpheres().Spheres().size(); ++i) {
            Add(static_cast<int>(triggers.RacerSpheres().Inside(i)),
                triggers.RacerSpheres().IsActive(i));
        }
        for (std::size_t i = 0; i < triggers.EnvironmentSpheres().Spheres().size(); ++i) {
            Add(triggers.EnvironmentSpheres().IsActive(i));
        }
        const TrackTimers& timers = triggers.Timers();
        for (std::size_t i = 0; i < timers.Count(); ++i) {
            Add(timers.IsOn(i), timers.PhaseTime(i).value_or(Milliseconds{-1}));
        }
    }

    void Add(const TrackSurfaces& surfaces) {
        for (std::size_t i = 0; i < surfaces.Count(); ++i) {
            Add(surfaces.IsSolid(i), surfaces.LetsShotsThrough(i));
        }
    }

    void Add(const TriggerWorlds& worlds) {
        for (const TriggerWorld& slot : worlds.Worlds()) {
            Add(static_cast<std::uint64_t>(slot.world), slot.left);
        }
    }

    void Add(const WheelSurfaces& under) {
        for (std::size_t wheel = 0; wheel < under.size(); ++wheel) {
            if (under.at(wheel)) {
                Add(static_cast<std::uint64_t>(wheel),
                    static_cast<std::uint64_t>(*under.at(wheel)));
            }
        }
    }

    void Add(const EntityClock& clock) {
        Add(clock.has_value());
        if (clock) {
            Add(static_cast<std::uint64_t>(clock->part), clock->frame, clock->loops);
        }
    }

    void Add(const TrackEntities& entities) {
        for (std::size_t i = 0; i < entities.Clocks().size(); ++i) {
            Add(entities.Clocks()[i], entities.Queued()[i], entities.IsFrozen(i));
        }
    }

    void Add(ResourcePhase phase) { Add(static_cast<std::uint64_t>(phase)); }

    void Add(const EventResources& resources) {
        const auto addAll = [this](const auto& items) {
            for (const auto& item : items) {
                Add(item);
            }
        };
        addAll(resources.PartAnimations());
        for (const TimerState& timer : resources.Timers()) {
            Add(timer.phase, timer.left, timer.isHeld);
        }
        addAll(resources.NodeTransforms());
        addAll(resources.ModelDistances());
        addAll(resources.SkyStates());
        for (const std::optional<Placement>& moved : resources.MovedWorlds()) {
            if (moved) {
                Add(moved->position, moved->basis.forward, moved->basis.up);
            }
        }
    }

    void Add(const HammerSwish& hammer) { Add(hammer.isArmed); }
    void Add(const Crane& crane) { Add(crane.isArmed, crane.ball); }
    void Add(const HazardBox& box) { Add(box.box.centre, box.box.axes); }
    void Add(const Ghost& ghost) { Add(ghost.reach.centre, ghost.nearWait); }
    void Add(const FallingPillar& pillar) { Add(pillar.hasLanded); }
    void Add(const Sphinx& sphinx) { Add(sphinx.isDestroyed, sphinx.blowUpLeft); }
    void Add(const Breakable& prop) { Add(prop.isBroken); }

    void Add(const HazardCannon& cannon) {
        Add(cannon.isFlying, cannon.target.position, cannon.flight.Position(),
            cannon.flight.Remaining());
    }
    void Add(const LavaBlob& blob) { Add(blob.reach.centre, blob.eventWait, blob.isBubbling); }
    void Add(const RocketPad& pad) { Add(pad.isOn, pad.hasShieldedRacer); }
    void Add(const CurseDrop& /*drop*/) {}
    void Add(const LightningCannon& cannon) { Add(cannon.sweepTime, cannon.cooldown); }
    void Add(const DragonCannonballs& dragon) { Add(dragon.wait); }
    void Add(const WarpPad& /*pad*/) {}
    void Add(const SmokeVent& /*vent*/) {}
    void Add(const Snowfall& /*snowfall*/) {}

    void Add(const CodePuzzle& puzzle) {
        for (std::size_t gate = 0; gate < kCodeGates; ++gate) {
            Add(static_cast<int>(puzzle.code[gate]), static_cast<int>(puzzle.lampCode[gate]));
        }
        Add(static_cast<int>(puzzle.progress), puzzle.revealLeft);
    }

    void Add(const UfoGrabber& ufo) {
        Add(ufo.reach.centre, static_cast<int>(ufo.phase), ufo.held ? int{*ufo.held} : -1,
            ufo.phaseLeft, ufo.watchdog);
    }

    void Add(const Hazards& hazards) {
        for (const bool isHidden : hazards.Models().Hidden()) {
            Add(isHidden);
        }
        for (const HazardAim& aim : hazards.Aims()) {
            Add(aim.source, aim.target, aim.direction);
        }
        for (const HazardSlot& slot : hazards.Slots()) {
            Add(static_cast<int>(slot.phase));
            std::visit(
                [this](const auto& hazard) {
                    if constexpr (!std::same_as<std::decay_t<decltype(hazard)>, std::monostate>) {
                        Add(hazard);
                    }
                },
                slot.state);
        }
    }

    void Add(const RaceClock& clock) {
        Add(static_cast<int>(clock.Phase()), clock.PhaseTime(), clock.RaceTime());
    }

    template <typename... Fields>
        requires(sizeof...(Fields) > 1)
    void Add(const Fields&... fields) {
        (Add(fields), ...);
    }

private:
    std::uint64_t value_ = kFnvOffset;
};

}

std::uint64_t HashSnapshot(const RenderSnapshot& snapshot) {
    std::uint64_t hash = Mix(kFnvOffset, snapshot.tick);
    for (const EntitySnapshot& entity : snapshot.entities) {
        hash = Mix(hash, static_cast<std::uint32_t>(entity.id));
        hash = MixTransform(hash, entity.transform);
        hash = Mix(hash, static_cast<std::uint8_t>(entity.flags));
    }
    hash = MixTransform(hash, snapshot.camera.transform);
    hash = Mix(hash, static_cast<std::uint8_t>(snapshot.camera.flags));
    for (const CameraSnapshot& camera : snapshot.otherPlayerCameras) {
        hash = MixTransform(hash, camera.transform);
        hash = Mix(hash, static_cast<std::uint8_t>(camera.flags));
    }
    return Mix(hash, snapshot.simStateHash);
}

std::uint64_t HashRacerState(const Racer& racer) {
    StateHash hash;
    hash.Add(racer.Body(), racer.Driver(), racer.Boost(), racer.ActiveTurbo(), racer.Status());
    return hash.Value();
}

std::uint64_t HashRaceSession(const RaceSession& session) {
    StateHash hash;
    hash.Add(session.Clock());
    for (std::size_t i = 0; i < session.Racers().size(); ++i) {
        const Racer& racer = session.Racers()[i];
        hash.Add(HashRacerState(racer), racer.Route(), session.Progress().OfRacer(i),
                 session.WasRespawned(i), session.SwitchedWorlds()[i],
                 session.SurfaceContacts().Under(i), session.Taunters()[i].Wait(),
                 session.DriverOf(i));
        const std::optional<std::size_t> tint = session.Tints().Of(i);
        hash.Add(tint.has_value(), static_cast<std::uint64_t>(tint.value_or(0)));
    }
    for (const std::size_t racer : session.Progress().Standings()) {
        hash.Add(static_cast<std::uint64_t>(racer));
    }
    hash.Add(session.Powerups(), session.Blasts(), session.Spikes(), session.Triggers(),
             session.Surfaces(), session.Entities(), session.Resources(), session.TrackHazards());
    if (!session.Bricks().IsEmpty()) {
        hash.Add(session.Bricks());
        for (const PowerupInventory& inventory : session.Inventories()) {
            hash.Add(inventory);
        }
    }
    return hash.Value();
}

std::vector<std::byte> EncodeSnapshotHashes(std::span<const std::uint64_t> hashes) {
    std::vector<std::byte> out;
    AppendHeader(out, kMagic, kVersion);
    for (const std::uint64_t hash : hashes) {
        AppendU64(out, hash);
    }
    AppendChecksum(out);
    return out;
}

Result<std::vector<std::uint64_t>> DecodeSnapshotHashes(std::span<const std::byte> bytes) {
    Result<assets::ByteReader> reader =
        OpenChecked(bytes, kMagic, kVersion, kVersion, "snapshot hashes")
            .transform([](const CheckedBytes& checked) { return checked.reader; });
    if (!reader) {
        return std::unexpected(std::move(reader.error()));
    }
    if (reader->Remaining() % kHashBytes != 0) {
        return Fail(ErrorCode::InvalidData, "snapshot hashes end mid-hash");
    }
    std::vector<std::uint64_t> hashes;
    while (reader->Remaining() != 0) {
        hashes.push_back(*ReadU64(*reader));
    }
    return hashes;
}

}
