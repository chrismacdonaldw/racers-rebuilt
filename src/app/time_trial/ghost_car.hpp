#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <utility>

#include "app/racer/posed_draws.hpp"
#include "app/racer/racer_assets.hpp"
#include "app/racer/racer_draws.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"
#include "sim/time_trial/ghost_playback.hpp"

namespace rr::app {

/// `seat` is where the driver sits in the chassis's frame, in game units.
struct GhostCarModels {
    ModelDraws chassis;
    const JointedAssets* chassisJoints = nullptr;
    ModelDraws body;
    ModelDraws driver;
    const JointedAssets* driverJoints = nullptr;
    Vector3 seat;
    std::size_t driverPart = 0;
};

/// A ghost's chassis, body and driver, drawn half transparent. The joints must outlive it.
class GhostCar {
public:
    /// Fails when the driver has no part `driverPart`, or a model doesn't fit its skeleton.
    [[nodiscard]] static Result<GhostCar> Create(GhostCarModels models);

    /// Hides the car while `frame` is nothing.
    void Update(const std::optional<sim::GhostFrame>& frame, float elapsedMs);

    /// In engine space, and empty while hidden.
    [[nodiscard]] std::span<const render::MeshDraw> Draws() const;

private:
    struct Parts {
        AnimatedSkeleton chassis;
        AnimatedSkeleton driver;
        PosedDraws posed;
        PosedDraws::Group chassisDraws;
        PosedDraws::Group bodyDraws;
        PosedDraws::Group driverDraws;
        Vector3 seat;
    };

    explicit GhostCar(Parts parts) : parts_(std::move(parts)) {}

    Parts parts_;
    bool isShown_ = false;
};

}
