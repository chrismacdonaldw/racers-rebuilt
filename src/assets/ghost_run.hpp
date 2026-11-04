#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"

namespace rr::assets {

inline constexpr std::size_t kGhostLapCount = 3;
inline constexpr std::size_t kGhostSampleCapacity = 961;

using GhostLapTimes = std::array<Milliseconds, kGhostLapCount>;

/// A game-space position × 32 and a stored x y z w rotation × 127, each truncated toward zero.
struct GhostSample {
    std::array<std::int16_t, 3> position{};
    std::array<std::int8_t, 4> rotation{};

    friend bool operator==(const GhostSample&, const GhostSample&) = default;
};

struct GhostRun {
    GhostLapTimes laps{};
    Transform start;
    std::vector<GhostSample> samples;
};

[[nodiscard]] Milliseconds RaceTime(const GhostRun& run);

/// `pose` is in game space.
[[nodiscard]] GhostSample ToGhostSample(const Transform& pose);

/// In game space.
[[nodiscard]] Transform FromGhostSample(const GhostSample& sample);

[[nodiscard]] Result<GhostRun> ParseGhb(std::span<const std::byte> bytes);

[[nodiscard]] GhostRun Mirrored(GhostRun run);

}
