#include "sim/race/checkpoint_graph.hpp"

#include <algorithm>
#include <format>
#include <limits>
#include <utility>

namespace rr::sim {
namespace {

constexpr float kNoFraction = -1.0F;

using Checkpoints = std::vector<assets::Checkpoint>;

Result<std::vector<std::size_t>> MainLoop(const Checkpoints& checkpoints) {
    std::vector<std::size_t> loop;
    std::size_t index = 0;
    do {
        if (loop.size() == checkpoints.size()) {
            return Fail(ErrorCode::InvalidData, "first links never return to checkpoint 0");
        }
        loop.push_back(index);
        index = checkpoints.at(index).links[0];
    } while (index != 0);
    return loop;
}

Result<void> FillBranch(const Checkpoints& checkpoints, std::size_t source, std::size_t target,
                        std::vector<float>& fractions) {
    std::vector<std::size_t> branch;
    std::size_t index = target;
    while (fractions[index] == kNoFraction) {
        if (branch.size() == checkpoints.size()) {
            return Fail(ErrorCode::InvalidData,
                        std::format("the branch from checkpoint {} never rejoins", source));
        }
        branch.push_back(index);
        index = checkpoints.at(index).links[0];
    }
    const float from = fractions[source];
    const float step = (fractions[index] - from) / static_cast<float>(branch.size() + 1);
    for (std::size_t i = 0; i < branch.size(); ++i) {
        fractions[branch[i]] = from + (step * static_cast<float>(i + 1));
    }
    return {};
}

Result<std::vector<float>> LapFractions(const Checkpoints& checkpoints,
                                        const std::vector<std::size_t>& loop) {
    std::vector<float> fractions(checkpoints.size(), kNoFraction);
    for (std::size_t step = 0; step < loop.size(); ++step) {
        fractions[loop[step]] = static_cast<float>(step) / static_cast<float>(loop.size());
    }
    for (const std::size_t source : loop) {
        const auto& links = checkpoints[source].links;
        for (std::size_t slot = 1; slot < links.size(); ++slot) {
            if (links.at(slot) == assets::kNoLink) {
                continue;
            }
            Result<void> filled = FillBranch(checkpoints, source, links.at(slot), fractions);
            if (!filled) {
                return std::unexpected(std::move(filled.error()));
            }
        }
    }
    const auto unreached = std::ranges::find(fractions, kNoFraction);
    if (unreached != fractions.end()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("checkpoint {} is off every route",
                                std::distance(fractions.begin(), unreached)));
    }
    return fractions;
}

}

float SignedDistance(const assets::Checkpoint& gate, const Vector3& point) {
    return Dot(gate.normal, point) + gate.distance;
}

Result<CheckpointGraph> CheckpointGraph::Create(assets::CheckpointsAsset asset) {
    return MainLoop(asset.checkpoints)
        .and_then([&](const std::vector<std::size_t>& loop) {
            return LapFractions(asset.checkpoints, loop);
        })
        .transform([&](std::vector<float> fractions) {
            return CheckpointGraph(std::move(asset.checkpoints), std::move(fractions));
        });
}

float CheckpointGraph::LapFraction(std::size_t checkpoint) const {
    return fractions_.at(checkpoint);
}

std::optional<GateCrossing> CheckpointGraph::FindCrossing(const Vector3& from,
                                                          const Vector3& to) const {
    std::optional<GateCrossing> nearest;
    float nearestAlong = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < checkpoints_.size(); ++i) {
        const float before = SignedDistance(checkpoints_[i], from);
        const float after = SignedDistance(checkpoints_[i], to);
        if ((before > 0.0F) == (after > 0.0F)) {
            continue;
        }
        const float along = before / (before - after);
        if (along >= nearestAlong ||
            Length(Lerp(from, to, along) - checkpoints_[i].position) > kGateRadius) {
            continue;
        }
        nearestAlong = along;
        nearest = GateCrossing{.checkpoint = i,
                               .side = before > 0.0F ? GateSide::Forward : GateSide::Backward};
    }
    return nearest;
}

float CheckpointGraph::DistanceToNextGate(std::size_t checkpoint, const Vector3& position) const {
    float nearest = std::numeric_limits<float>::max();
    for (const std::uint8_t link : checkpoints_.at(checkpoint).links) {
        if (link != assets::kNoLink) {
            nearest = std::min(nearest, SignedDistance(checkpoints_[link], position));
        }
    }
    return nearest;
}

}
