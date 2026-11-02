#pragma once

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "app/scene.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

inline constexpr std::string_view kCarViewerScenePrefix = "car:";

[[nodiscard]] bool IsCarViewerScene(std::string_view scene);

/// A racer's car from `DEFAULT.LRS` or `QBUILD.LRS` as the menus build it, turning on the spot.
class CarViewerScene final : public Scene {
public:
    /// `scene` is `car:<racer name>`, in any case. `renderer` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<CarViewerScene>> Create(render::Renderer& renderer,
                                                                        const assets::Vfs& vfs,
                                                                        assets::TrackLoader& loader,
                                                                        std::string_view scene);

    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    CarViewerScene(render::Renderer& renderer, std::vector<render::MeshDraw> draws,
                   float middleHeight)
        : renderer_(&renderer), draws_(std::move(draws)), middleHeight_(middleHeight) {}

    render::Renderer* renderer_;
    std::vector<render::MeshDraw> draws_;
    float middleHeight_;
    float turnRadians_ = 0.0F;
};

}
