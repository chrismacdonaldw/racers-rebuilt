#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <tuple>
#include <vector>

#include "app/menu/carousel_layout.hpp"
#include "assets/piece_catalog.hpp"
#include "assets/track_loader.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// Uploads each piece once, as only a scene change frees meshes. `catalog` must outlive it.
// NOLINTNEXTLINE(bugprone-exception-escape)
class PieceModels {
public:
    [[nodiscard]] static Result<PieceModels> Load(render::Renderer& renderer,
                                                  assets::TrackLoader& loader,
                                                  const assets::PieceCatalog& catalog);

    /// Footprint corner at the origin. Empty when the piece fails to build.
    [[nodiscard]] const PartPreview& Model(const assets::PieceRecord& piece, std::uint8_t colour,
                                           unsigned rotation);

    /// The set's choices in order, unturned.
    [[nodiscard]] std::span<const PartPreview> SetPreviews(std::size_t set);

private:
    using Key = std::tuple<const assets::PieceRecord*, std::uint8_t, unsigned>;

    PieceModels(render::Renderer& renderer, const assets::PieceCatalog& catalog,
                assets::NameMap<render::Material> materials);

    render::Renderer* renderer_;
    const assets::PieceCatalog* catalog_;
    assets::NameMap<render::Material> materials_;
    std::map<Key, PartPreview> models_;
    std::vector<std::vector<PartPreview>> sets_;
};

}
