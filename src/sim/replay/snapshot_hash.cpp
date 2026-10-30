#include "sim/replay/snapshot_hash.hpp"

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <optional>

#include "sim/replay/replay_bytes.hpp"

namespace rr::sim {
namespace {

constexpr std::string_view kMagic = "RRRH";
constexpr std::uint16_t kVersion = 1;
constexpr std::size_t kHashBytes = 8;

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

    void Add(const BodyState& body) {
        Add(body.motion, body.saved, body.angularVelocity, body.force, body.torque, body.facing,
            body.derived, body.turnRadius, body.yawRate, body.thrust, body.maxSpeed,
            body.dragCoefficient, body.gripScale, body.steering, body.impulses, body.hasWallYaw,
            body.hasWallContact, body.airborneMs, body.supportForce);
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
            Add(route->PlaybackSpeed(), route->Velocity(), route->ContactCount());
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
    return Mix(hash, snapshot.simStateHash);
}

std::uint64_t HashRacerState(const Racer& racer) {
    StateHash hash;
    hash.Add(racer.Body(), racer.Driver(), racer.Boost(), racer.ActiveTurbo());
    return hash.Value();
}

std::uint64_t HashRaceSession(const RaceSession& session) {
    StateHash hash;
    hash.Add(session.Clock());
    for (std::size_t i = 0; i < session.Racers().size(); ++i) {
        const Racer& racer = session.Racers()[i];
        hash.Add(HashRacerState(racer), racer.Route(), session.Progress().OfRacer(i),
                 session.WasRespawned(i));
    }
    for (const std::size_t racer : session.Progress().Standings()) {
        hash.Add(static_cast<std::uint64_t>(racer));
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
    Result<assets::ByteReader> reader = OpenChecked(bytes, kMagic, kVersion, "snapshot hashes");
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
