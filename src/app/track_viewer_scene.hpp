#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "app/scene.hpp"
#include "assets/track_loader.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// Starts above the track. The free camera enhancement flies around it.
class TrackViewerScene final : public Scene {
public:
    /// `track` is a folder under `GAMEDATA`, such as `RACEC0R0`. `renderer` must outlive the
    /// scene. Logs the time from `started` to the first drawn frame.
    [[nodiscard]] static Result<std::unique_ptr<TrackViewerScene>> Create(
        render::Renderer& renderer, assets::TrackLoader& loader, std::string_view track,
        Nanoseconds started);

    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    struct Parts {
        render::Renderer* renderer = nullptr;
        std::string track;
        std::vector<render::MeshDraw> draws;
        render::Camera camera;
        Nanoseconds started{};
    };

    explicit TrackViewerScene(Parts parts) : parts_(std::move(parts)) {}

    Parts parts_;
    bool hasDrawn_ = false;
};

}
