#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "app/scene.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

class SpriteTestScene final : public Scene {
public:
    /// `renderer` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<SpriteTestScene>> Create(
        render::Renderer& renderer);

    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    SpriteTestScene(render::Renderer& renderer, render::TextureId tile);

    render::Renderer* renderer_;
    render::TextureId tile_;
    std::vector<render::SpriteDraw> sprites_;
    std::uint64_t tick_ = 0;
};

}
