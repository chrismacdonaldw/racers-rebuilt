#include "app/brick_models.hpp"

#include <array>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "app/track_model.hpp"
#include "assets/model_files.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kCommonFolder = "GAMEDATA/COMMON";
constexpr std::string_view kPowerupLibrary = "POWERUP";
constexpr std::string_view kColourBrick = "PUBRICKY";
constexpr std::string_view kColourTrail = "PUTRAILY";
constexpr std::string_view kWhiteBrick = "ENHABRIK";
constexpr std::string_view kWhiteTrail = "ENHANER";
constexpr std::string_view kStockMaterial = "PTRAILM";
constexpr std::array<char, kBrickColourCount> kColourLetters{'P', 'S', 'T', 'M'};

using Draws = std::vector<render::MeshDraw>;

assets::ModelFiles PowerupModel(std::string_view geometry) {
    return {.materials = std::string(kPowerupLibrary),
            .textures = std::string(kPowerupLibrary),
            .geometry = std::string(geometry)};
}

Result<void> Append(Result<Draws> draws, Draws& out) {
    return draws.transform(
        [&](const Draws& uploaded) { out.insert(out.end(), uploaded.begin(), uploaded.end()); });
}

Result<Draws> UploadRecoloured(render::Renderer& renderer, const assets::TrackAssets& assets,
                               std::string_view model, const std::string& material) {
    const auto found = assets.materials.find(material);
    if (found == assets.materials.end()) {
        return Fail(ErrorCode::NotFound, std::format("no material named {}", material));
    }
    assets::TrackAssets recoloured = assets;
    recoloured.materials.insert_or_assign(std::string(kStockMaterial), found->second);
    return UploadModel(renderer, recoloured, model);
}

Result<Draws> UploadColour(render::Renderer& renderer, const assets::TrackAssets& assets,
                           char letter) {
    Draws draws;
    return Append(UploadRecoloured(renderer, assets, kColourBrick, std::format("PBRICK{}", letter)),
                  draws)
        .and_then([&] {
            return Append(
                UploadRecoloured(renderer, assets, kColourTrail, std::format("PTRAIL{}", letter)),
                draws);
        })
        .transform([&] { return std::move(draws); });
}

Result<Draws> UploadWhite(render::Renderer& renderer, const assets::TrackAssets& assets) {
    Draws draws;
    return Append(UploadModel(renderer, assets, kWhiteBrick), draws)
        .and_then([&] { return Append(UploadModel(renderer, assets, kWhiteTrail), draws); })
        .transform([&] { return std::move(draws); });
}

}

Result<BrickModels> BrickModels::Load(render::Renderer& renderer, assets::TrackLoader& loader) {
    const std::array models{PowerupModel(kColourBrick), PowerupModel(kColourTrail),
                            PowerupModel(kWhiteBrick), PowerupModel(kWhiteTrail)};
    Result<assets::TrackAssets> assets = loader.LoadModels(kCommonFolder, models);
    if (!assets) {
        return std::unexpected(std::move(assets.error()));
    }
    std::array<Draws, kBrickColourCount> coloured;
    for (std::size_t i = 0; i < kBrickColourCount; ++i) {
        Result<Draws> draws = UploadColour(renderer, *assets, kColourLetters.at(i));
        if (!draws) {
            return std::unexpected(std::move(draws.error()));
        }
        coloured.at(i) = std::move(*draws);
    }
    return UploadWhite(renderer, *assets).transform([&](Draws white) {
        return BrickModels(std::move(coloured), std::move(white));
    });
}

void BrickModels::AppendDraws(std::span<const sim::BrickSnapshot> bricks,
                              std::vector<render::MeshDraw>& out) const {
    for (const sim::BrickSnapshot& brick : bricks) {
        const Draws& model =
            brick.colour ? coloured_.at(static_cast<std::size_t>(*brick.colour)) : white_;
        for (render::MeshDraw draw : model) {
            draw.transform = brick.transform;
            draw.scale = brick.scale;
            out.push_back(draw);
        }
    }
}

}
