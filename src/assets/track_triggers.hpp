#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/event_id.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

enum class TriggerList : std::uint8_t { Racer, Environment };

/// `collisionWorld`, `lap` and `isForProjectiles` appear only in the racer list.
struct TriggerSphere {
    Vector3 centre;
    float radius = 0.0F;
    EventId event = 0;
    bool isPlayerOnly = false;
    bool isForProjectiles = false;
    std::optional<std::string> collisionWorld;
    std::optional<std::int32_t> lap;
};

/// A random phase lasts a fresh draw from 0 up to `length` each time.
struct TimerPhase {
    Milliseconds length{0};
    bool isRandom = false;
};

struct TrackTimer {
    TimerPhase on;
    TimerPhase off;
    EventId event = kNoEvent;
    Milliseconds startDelay{0};
};

/// Reads `MAINTRIG.TRB` as the racer list and `ENVIRON.TRB` as the environment list.
[[nodiscard]] Result<std::vector<TriggerSphere>> ParseTriggerSpheres(
    std::span<const std::byte> bytes, TriggerList list);

[[nodiscard]] Result<std::vector<TrackTimer>> ParseTrackTimers(std::span<const std::byte> bytes);

}
