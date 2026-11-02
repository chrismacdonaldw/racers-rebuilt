#include "app/track_map.hpp"

#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "assets/material_library.hpp"
#include "assets/race_map.hpp"
#include "assets/world_database.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kMapLibrary = 1;

std::string BinaryName(std::string_view name, std::string_view extension) {
    return assets::ToUpperAscii(name.substr(0, name.find('.'))) + std::string(extension);
}

Result<std::string> MapTextureName(const assets::Vfs& vfs, std::string_view folder,
                                   std::string_view world) {
    return vfs.Read(std::format("{}/{}", folder, BinaryName(world, ".WDB")))
        .and_then([](const assets::FileData& file) { return assets::ParseWorld(file.Bytes()); })
        .and_then([&](const assets::WorldAsset& asset) -> Result<assets::FileData> {
            if (asset.materialLibraries.size() <= kMapLibrary) {
                return Fail(ErrorCode::NotFound, std::format("{} has no map library", world));
            }
            return vfs.Read(std::format("{}/{}", folder,
                                        BinaryName(asset.materialLibraries[kMapLibrary], ".MDB")));
        })
        .and_then(
            [](const assets::FileData& file) { return assets::ParseMaterialLibrary(file.Bytes()); })
        .and_then([&](const std::vector<assets::MaterialAsset>& materials) -> Result<std::string> {
            if (materials.empty()) {
                return Fail(ErrorCode::NotFound, std::format("{}'s map library is empty", world));
            }
            return assets::ToUpperAscii(materials.front().texture);
        });
}

}

std::optional<TrackMap> LoadTrackMap(const assets::Vfs& vfs, std::string_view track,
                                     bool isMirrored,
                                     const assets::NameMap<render::TextureId>& textures) {
    const std::string folder = std::format("GAMEDATA/{}", track);
    Result<assets::RaceMapFields> fields =
        vfs.Read(std::format("{}/{}.RAB", folder, track)).and_then([](const assets::FileData& rab) {
            return assets::ReadRaceMapFields(rab.Bytes());
        });
    if (!fields || !fields->bounds) {
        log::Warn("{}: {}, racing without a map", track,
                  fields ? std::string("no map bounds") : fields.error().message);
        return std::nullopt;
    }
    TrackMap map{.minX = fields->bounds->minX,
                 .maxY = fields->bounds->maxY,
                 .maxX = fields->bounds->maxX,
                 .minY = fields->bounds->minY,
                 .isMirrored = isMirrored};
    if (!fields->world) {
        return map;
    }
    Result<std::string> texture = MapTextureName(vfs, folder, *fields->world);
    const auto uploaded = texture ? textures.find(*texture) : textures.end();
    if (uploaded == textures.end()) {
        log::Warn("{}: {}, the map has no image", track,
                  texture ? std::format("{} isn't loaded", *texture) : texture.error().message);
        return map;
    }
    map.image = uploaded->second;
    return map;
}

}
