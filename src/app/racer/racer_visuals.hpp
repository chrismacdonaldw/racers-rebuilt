#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "app/racer/animation_player.hpp"
#include "app/racer/body_lean.hpp"
#include "app/racer/driver_moves.hpp"
#include "app/racer/posed_draws.hpp"
#include "app/racer/racer_assets.hpp"
#include "app/racer/racer_draws.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "render/render_view.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// Poses a racer's wheels, body and driver each rendered frame. `assets` must outlive it.
class RacerVisuals {
public:
    /// Fails when a draw follows a node the skeleton lacks, or a needed animation is missing.
    [[nodiscard]] static Result<RacerVisuals> Create(const RacerAssets& assets, RacerDraws draws,
                                                     sim::Random random);

    /// In game space: `wheels` is the physics pose and `nearestRacer` the closest other racer.
    void Update(const Transform& wheels, const sim::RacerSnapshot& motion,
                std::optional<Vector3> nearestRacer, float elapsedMs);

    /// In engine space.
    [[nodiscard]] std::span<const render::MeshDraw> Draws() const { return parts_.posed.Draws(); }

private:
    struct WheelParts {
        std::size_t forward = 0;
        std::size_t reverse = 0;
    };

    using DriverParts = std::array<std::size_t, kDriverPartCount>;

    struct Parts {
        AnimatedSkeleton wheels;
        AnimatedSkeleton driver;
        WheelParts wheelParts;
        DriverParts driverParts;
        PosedDraws posed;
        PosedDraws::Group wheelDraws;
        PosedDraws::Group bodyDraws;
        PosedDraws::Group driverDraws;
        Vector3 seat;
        float mass = 0.0F;
    };

    [[nodiscard]] static Result<WheelParts> FindWheelParts(const AnimationPlayer& player);
    [[nodiscard]] static Result<DriverParts> FindDriverParts(const AnimationPlayer& player);

    RacerVisuals(Parts parts, sim::Random random) : parts_(std::move(parts)), moves_(random) {}

    void Spin(float forwardSpeed, float elapsedMs);
    void Drive(const DriverSense& sense, float elapsedMs);

    Parts parts_;
    BodyLean lean_;
    DriverMoves moves_;
    std::uint32_t hitReactions_ = 0;
    std::uint32_t suddenStops_ = 0;
};

}
