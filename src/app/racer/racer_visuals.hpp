#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "app/racer/animation_player.hpp"
#include "app/racer/body_lean.hpp"
#include "app/racer/racer_assets.hpp"
#include "app/racer/racer_draws.hpp"
#include "app/racer/skeleton_pose.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "render/render_view.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// Poses a racer's wheels, body and driver each rendered frame. `assets` must outlive it.
class RacerVisuals {
public:
    /// Fails when a draw follows a node the skeleton lacks, or a needed animation is missing.
    [[nodiscard]] static Result<RacerVisuals> Create(const RacerAssets& assets, RacerDraws draws);

    /// `wheels` is the racer's game-space physics pose.
    void Update(const Transform& wheels, const sim::RacerSnapshot& motion, float elapsedMs);

    /// In engine space.
    [[nodiscard]] std::span<const render::MeshDraw> Draws() const { return parts_.draws; }

private:
    struct Group {
        std::size_t first = 0;
        std::size_t count = 0;
    };

    struct Animated {
        SkeletonPose pose;
        AnimationPlayer player;
    };

    struct WheelParts {
        std::size_t forward = 0;
        std::size_t reverse = 0;
    };

    struct DriverParts {
        std::size_t straight = 0;
        std::size_t steerRight = 0;
        std::size_t steerLeft = 0;
    };

    struct Parts {
        Animated wheels;
        Animated driver;
        WheelParts wheelParts;
        DriverParts driverParts;
        std::vector<render::MeshDraw> draws;
        std::vector<std::uint16_t> joints;
        Group wheelDraws;
        Group bodyDraws;
        Group driverDraws;
        Vector3 seat;
        float mass = 0.0F;

        Group Append(const ModelDraws& model);
    };

    [[nodiscard]] static Result<WheelParts> FindWheelParts(const AnimationPlayer& player);
    [[nodiscard]] static Result<DriverParts> FindDriverParts(const AnimationPlayer& player);

    explicit RacerVisuals(Parts parts) : parts_(std::move(parts)) {}

    void Spin(float forwardSpeed, float elapsedMs);
    void Steer(float turnRadius, float elapsedMs);
    void Place(Group group, const Transform& entity, std::span<const Transform> nodes);

    Parts parts_;
    BodyLean lean_;
};

}
