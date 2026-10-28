#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "assets/checkpoints.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::sim {

/// How far from a checkpoint's position a crossing of its plane counts as passing the gate.
inline constexpr float kGateRadius = 80.0F;

enum class GateSide : std::uint8_t { Forward, Backward };

struct GateCrossing {
    std::size_t checkpoint = 0;
    GateSide side = GateSide::Forward;

    friend constexpr bool operator==(const GateCrossing&, const GateCrossing&) = default;
};

class CheckpointGraph {
public:
    /// Fails unless every checkpoint leads back to the loop through checkpoint 0.
    [[nodiscard]] static Result<CheckpointGraph> Create(assets::CheckpointsAsset asset);

    [[nodiscard]] std::span<const assets::Checkpoint> Checkpoints() const { return checkpoints_; }

    [[nodiscard]] float LapFraction(std::size_t checkpoint) const;

    /// The first gate crossed on the way from `from` to `to`.
    [[nodiscard]] std::optional<GateCrossing> FindCrossing(const Vector3& from,
                                                           const Vector3& to) const;

    /// How far `position` is behind the nearest gate linked from `checkpoint`.
    [[nodiscard]] float DistanceToNextGate(std::size_t checkpoint, const Vector3& position) const;

private:
    CheckpointGraph(std::vector<assets::Checkpoint> checkpoints, std::vector<float> fractions)
        : checkpoints_(std::move(checkpoints)), fractions_(std::move(fractions)) {}

    std::vector<assets::Checkpoint> checkpoints_;
    std::vector<float> fractions_;
};

/// Positive behind the gate, negative past it.
[[nodiscard]] float SignedDistance(const assets::Checkpoint& gate, const Vector3& point);

}
