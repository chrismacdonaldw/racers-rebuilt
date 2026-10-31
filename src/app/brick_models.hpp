#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "assets/track_loader.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

inline constexpr std::size_t kBrickColourCount = 4;

class BrickModels {
public:
    [[nodiscard]] static Result<BrickModels> Load(render::Renderer& renderer,
                                                  assets::TrackLoader& loader);

    void AppendDraws(std::span<const sim::BrickSnapshot> bricks,
                     std::vector<render::MeshDraw>& out) const;

private:
    using Draws = std::vector<render::MeshDraw>;

    BrickModels(std::array<Draws, kBrickColourCount> coloured, Draws white)
        : coloured_(std::move(coloured)), white_(std::move(white)) {}

    std::array<Draws, kBrickColourCount> coloured_;
    Draws white_;
};

}
