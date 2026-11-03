#include "app/menu/piece_models.hpp"

#include <string>
#include <string_view>
#include <utility>

#include "app/racer/racer_bounds.hpp"
#include "app/track_model.hpp"
#include "core/log.hpp"
#include "sim/car_build/car_mesh.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kPieceFolder = "MENUDATA/PIECEDB";
constexpr std::string_view kPieceLooks = "LPIECEHI";

}

Result<PieceModels> PieceModels::Load(render::Renderer& renderer, assets::TrackLoader& loader,
                                      const assets::PieceCatalog& catalog) {
    const assets::ModelFiles files{.materials = std::string(kPieceLooks),
                                   .textures = std::string(kPieceLooks)};
    return loader.LoadModels(kPieceFolder, std::span(&files, 1))
        .and_then([&](const assets::TrackAssets& looks) {
            return UploadMaterials(renderer, looks, catalog.materials);
        })
        .transform([&](assets::NameMap<render::Material> materials) {
            return PieceModels(renderer, catalog, std::move(materials));
        });
}

PieceModels::PieceModels(render::Renderer& renderer, const assets::PieceCatalog& catalog,
                         assets::NameMap<render::Material> materials)
    : renderer_(&renderer),
      catalog_(&catalog),
      materials_(std::move(materials)),
      sets_(catalog.partSets.size()) {}

const PartPreview& PieceModels::Model(const assets::PieceRecord& piece, std::uint8_t colour,
                                      unsigned rotation) {
    const Key key{&piece, colour, rotation};
    if (const auto found = models_.find(key); found != models_.end()) {
        return found->second;
    }
    PartPreview& model = models_[key];
    Result<assets::GeometryAsset> geometry =
        sim::BuildPieceMesh(piece, colour, rotation, *catalog_);
    Result<JointedModel> uploaded = geometry.and_then([&](const assets::GeometryAsset& built) {
        return UploadJointedModel(*renderer_, built, materials_);
    });
    if (!uploaded) {
        log::Warn("{}, the car builder leaves piece {} out", uploaded.error().message, piece.name);
        return model;
    }
    model = {.draws = std::move(uploaded->draws), .bounds = ModelBounds(geometry->vertices)};
    return model;
}

std::span<const PartPreview> PieceModels::SetPreviews(std::size_t set) {
    std::vector<PartPreview>& previews = sets_.at(set);
    if (!previews.empty()) {
        return previews;
    }
    for (const assets::PartChoice& choice : catalog_->partSets.at(set).choices) {
        previews.push_back(Model(catalog_->library.pieces.at(choice.piece), choice.colour, 0));
    }
    return previews;
}

}
