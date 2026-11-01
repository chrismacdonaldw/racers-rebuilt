#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "assets/circuit_database.hpp"
#include "assets/race_list.hpp"

namespace rr::sim {

enum class CircuitAward : std::uint8_t { Gold, Silver, Bronze, None };

/// `place` counts from 1. No place scores 0.
[[nodiscard]] std::int32_t PlacePoints(std::optional<int> place);

/// Points are indexed by grid slot.
class CircuitRun {
public:
    CircuitRun(std::array<bool, assets::kCircuitRaces> hasRace, std::size_t player,
               bool isSoloPlayer);

    /// The race slot to run next, or nothing once the circuit is over.
    [[nodiscard]] std::optional<std::size_t> Race() const { return race_; }

    /// Set once the circuit is over.
    [[nodiscard]] std::optional<CircuitAward> Award() const { return award_; }

    [[nodiscard]] std::span<const std::int32_t> Points() const { return points_; }

    /// How many racers have strictly more points than `slot`.
    [[nodiscard]] std::size_t Rank(std::size_t slot) const;

    /// `places` holds each slot's finishing place, counted from 1.
    void FinishRace(std::span<const std::optional<int>> places);

    void Restart();

private:
    void Enter(std::size_t race);

    std::array<bool, assets::kCircuitRaces> hasRace_;
    std::size_t player_;
    bool isSoloPlayer_;
    std::array<std::int32_t, assets::kGridSlots> points_{};
    std::optional<std::size_t> race_;
    std::optional<CircuitAward> award_;
};

}
