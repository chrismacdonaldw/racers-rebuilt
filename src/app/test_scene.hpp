#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "app/scene.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

class TestScene final : public Scene {
public:
    /// `renderer` must outlive the scene.
    [[nodiscard]] static Result<std::unique_ptr<TestScene>> Create(render::Renderer& renderer);

    void Tick(const sim::InputFrame& input) override;
    void Snapshot(sim::RenderSnapshot& out) const override;
    void Render(const sim::RenderSnapshot& interpolated) override;

private:
    TestScene(render::Renderer& renderer, render::MeshDraw cube);

    render::Renderer* renderer_;
    std::array<render::MeshDraw, 1> meshes_;
    std::vector<render::DebugLine> lines_;
    std::uint64_t tick_ = 0;
};

}
