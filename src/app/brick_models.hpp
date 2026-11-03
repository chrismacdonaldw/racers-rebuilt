#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "app/far_models.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

inline constexpr std::size_t kBrickColourCount = 4;

class BrickModels {
public:
    [[nodiscard]] static Result<BrickModels> Load(render::Renderer& renderer,
                                                  const assets::Vfs& vfs,
                                                  assets::TrackLoader& loader);

    void AppendDraws(std::span<const sim::BrickSnapshot> bricks, std::vector<render::MeshDraw>& out,
                     FarModels& far) const;

private:
    using Draws = std::vector<render::MeshDraw>;

    BrickModels(std::array<Draws, kBrickColourCount> coloured, Draws white,
                float colouredRangeSquared, float whiteRangeSquared)
        : coloured_(std::move(coloured)),
          white_(std::move(white)),
          colouredRangeSquared_(colouredRangeSquared),
          whiteRangeSquared_(whiteRangeSquared) {}

    std::array<Draws, kBrickColourCount> coloured_;
    Draws white_;
    float colouredRangeSquared_;
    float whiteRangeSquared_;
};

}
