#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/pace_car.hpp"
#include "app/scene.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/race/checkpoint_graph.hpp"
#include "sim/race/lap_tracker.hpp"

namespace rr::app {

/// A pace car laps the track with the camera chasing it. The free camera enhancement flies around.
class TrackViewerScene final : public Scene {
public:
    /// `track` is a folder under `GAMEDATA`, such as `RACEC0R0`. `renderer` must outlive the
    /// scene. Logs the time from `started` to the first drawn frame.
    [[nodiscard]] static Result<std::unique_ptr<TrackViewerScene>> Create(
        render::Renderer& renderer, const assets::Vfs& vfs, assets::TrackLoader& loader,
        std::string_view track, Nanoseconds started);

    void ApplySettings(const settings::Registry& settings) override;
    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    struct Parts {
        render::Renderer* renderer = nullptr;
        std::string track;
        std::vector<render::MeshDraw> draws;
        std::size_t firstCarDraw = 0;
        PaceCar paceCar;
        render::Camera camera;
        std::vector<render::DebugLine> collisionLines;
        Nanoseconds started{};
        std::optional<sim::CheckpointGraph> checkpoints;
    };

    explicit TrackViewerScene(Parts parts) : parts_(std::move(parts)) {}

    void TrackLaps(const sim::CheckpointGraph& checkpoints, const Transform& before);

    Parts parts_;
    sim::LapTracker laps_;
    bool showsCheckpoints_ = false;
    bool showsCollision_ = false;
    std::vector<render::DebugLine> debugLines_;
    bool hasDrawn_ = false;
};

}
