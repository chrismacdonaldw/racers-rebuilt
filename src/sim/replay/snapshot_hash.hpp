#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/result.hpp"
#include "sim/physics/racer.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::sim {

/// Exact: any bit of difference in a transform changes it.
[[nodiscard]] std::uint64_t HashSnapshot(const RenderSnapshot& snapshot);

/// Covers what the pose leaves out, for `RenderSnapshot::simStateHash`.
[[nodiscard]] std::uint64_t HashRacerState(const Racer& racer);

[[nodiscard]] std::vector<std::byte> EncodeSnapshotHashes(std::span<const std::uint64_t> hashes);
[[nodiscard]] Result<std::vector<std::uint64_t>> DecodeSnapshotHashes(
    std::span<const std::byte> bytes);

}
