#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "app/race_camera.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "sim/race/race_session.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

inline constexpr float kSplitFovCutDegrees = 25.0F;

/// Stacks the players' views top to bottom, each the window's full width.
[[nodiscard]] render::Viewport PlayerViewport(std::size_t player, std::size_t players);

/// A split window's views are wider for their height, so each sees a narrower angle.
[[nodiscard]] render::Camera PlayerLens(render::Camera camera, std::size_t players);

[[nodiscard]] const Transform& PlayerCameraPose(const sim::RenderSnapshot& snapshot,
                                                std::size_t player);

/// Follows the players, who drive the first racer slots in order.
class PlayerCameras {
public:
    PlayerCameras(const sim::RaceSession& session, std::size_t players);

    [[nodiscard]] std::size_t Count() const { return cameras_.size(); }

    /// Call after each session tick.
    void Follow(const sim::RaceSession& session);

    /// In engine space, as of the last `Follow`.
    [[nodiscard]] std::span<const Transform> Poses() const { return poses_; }

    [[nodiscard]] Vector3 ViewDirection(std::size_t player) const {
        return cameras_.at(player).ViewDirection();
    }

    /// Needs the racers already in `out`.
    void Snapshot(sim::RenderSnapshot& out) const;

private:
    std::vector<RaceCamera> cameras_;
    std::vector<Transform> poses_;
};

}
