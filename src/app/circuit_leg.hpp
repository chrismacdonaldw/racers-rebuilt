#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "app/race_scene_name.hpp"
#include "assets/race_catalog.hpp"
#include "core/result.hpp"
#include "sim/race/circuit_run.hpp"

namespace rr::app {

/// One race of a circuit run, and the scene each way out of it leads to.
class CircuitLeg {
public:
    /// Fails when the circuit is missing, has no races, or has no race left to run.
    [[nodiscard]] static Result<CircuitLeg> Create(const assets::RaceCatalog& catalog,
                                                   const CircuitSceneSetup& setup);

    [[nodiscard]] const RaceSceneSetup& Race() const { return race_; }
    [[nodiscard]] std::span<const std::int32_t> Points() const { return run_.Points(); }

    /// `places` holds each grid slot's finishing place, counted from 1.
    [[nodiscard]] std::string AfterFinish(std::span<const std::optional<int>> places) const;
    [[nodiscard]] std::string AfterRestart() const;
    [[nodiscard]] std::string AfterQuit() const;

private:
    CircuitLeg(const sim::CircuitRun& run, RaceSceneSetup rules, RaceSceneSetup race);

    [[nodiscard]] std::string Next(const sim::CircuitRun& run) const;

    sim::CircuitRun run_;
    RaceSceneSetup rules_;
    RaceSceneSetup race_;
};

}
