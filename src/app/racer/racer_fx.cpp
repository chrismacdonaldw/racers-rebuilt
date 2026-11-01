#include "app/racer/racer_fx.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/track_model.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kCommonFolder = "GAMEDATA/COMMON";
constexpr std::string_view kLibrary = "RACERFX";

}

assets::NameMap<render::Material> LoadRacerFxMaterials(render::Renderer& renderer,
                                                       assets::TrackLoader& loader) {
    const std::array<assets::ModelFiles, 1> files{
        assets::ModelFiles{.materials = std::string(kLibrary), .textures = std::string(kLibrary)}};
    Result<assets::NameMap<render::Material>> materials =
        loader.LoadModels(kCommonFolder, files).and_then([&](const assets::TrackAssets& assets) {
            std::vector<std::string> names;
            names.reserve(assets.materials.size());
            for (const auto& [name, material] : assets.materials) {
                names.push_back(name);
            }
            return UploadMaterials(renderer, assets, names);
        });
    if (!materials) {
        log::Warn("racer effects: {}", materials.error().message);
        return {};
    }
    return std::move(*materials);
}

}
