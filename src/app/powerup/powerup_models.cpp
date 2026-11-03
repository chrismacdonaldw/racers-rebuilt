#include "app/powerup/powerup_models.hpp"

#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/powerup/powerup_quads.hpp"
#include "app/track_model.hpp"
#include "assets/game_path.hpp"
#include "assets/world_database.hpp"
#include "core/log.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kCommonFolder = "GAMEDATA/COMMON";
constexpr std::string_view kPowerupWorld = "POWERUP.WDB";
constexpr std::string_view kEmitterLibrary = "EMITTER";

const std::string* NameAt(const std::vector<std::string>& names, std::int32_t index) {
    return index >= 0 && static_cast<std::size_t>(index) < names.size()
               ? &names[static_cast<std::size_t>(index)]
               : nullptr;
}

std::vector<assets::ModelFiles> FilesOf(const assets::WorldAsset& world) {
    std::vector<assets::ModelFiles> files;
    files.reserve(world.models.size() + world.materialLibraries.size() + world.textureLists.size() +
                  1);
    for (const std::string& model : world.models) {
        files.push_back({.geometry = model});
    }
    for (const std::string& library : world.materialLibraries) {
        files.push_back({.materials = library});
    }
    for (const std::string& textures : world.textureLists) {
        files.push_back({.textures = textures});
    }
    files.push_back(
        {.materials = std::string(kEmitterLibrary), .textures = std::string(kEmitterLibrary)});
    return files;
}

Result<void> AddEntity(const assets::Vfs& vfs, const assets::WorldAsset& world,
                       const assets::WorldEntity& entity, EffectAssets& out) {
    if (entity.lods.empty()) {
        return {};
    }
    const assets::WorldLod& lod = entity.lods.front();
    const std::string* model = NameAt(world.models, lod.model);
    if (model == nullptr) {
        return {};
    }
    EffectModelFiles files{.geometry = *model,
                           .jointed = std::nullopt,
                           .placement = {.position = entity.position,
                                         .rotation = sim::ToQuaternion(
                                             sim::OrthonormalBasis(entity.forward, entity.up))},
                           .scroll = entity.textureScroll.value_or(assets::TextureScroll{}),
                           .rangeSquared = assets::DrawRangeSquared(entity)};
    if (const std::string* node = NameAt(world.sceneNodes, lod.sceneNode)) {
        Result<JointedAssets> jointed = LoadJointed(vfs, *node, *node);
        if (!jointed) {
            return Fail(jointed.error().code,
                        std::format("{}: {}", entity.name, jointed.error().message));
        }
        files.jointed = std::move(*jointed);
    }
    out.entities.insert_or_assign(assets::ToUpperAscii(entity.name.empty() ? *model : entity.name),
                                  std::move(files));
    return {};
}

Result<void> AddEntities(const assets::Vfs& vfs, const assets::WorldAsset& world,
                         std::span<const assets::WorldEntity> entities, EffectAssets& out) {
    for (const assets::WorldEntity& entity : entities) {
        if (Result<void> added = AddEntity(vfs, world, entity, out); !added) {
            return added;
        }
    }
    return {};
}

}

Result<EffectAssets> LoadEffectAssets(const assets::Vfs& vfs, assets::TrackLoader& loader,
                                      std::string_view world) {
    Result<assets::WorldAsset> parsed = vfs.Read(std::format("{}/{}", kCommonFolder, world))
                                            .and_then([](const assets::FileData& data) {
                                                return assets::ParseWorld(data.Bytes());
                                            });
    if (!parsed) {
        return std::unexpected(std::move(parsed.error()));
    }
    EffectAssets out;
    return loader.LoadModels(kCommonFolder, FilesOf(*parsed))
        .and_then([&](assets::TrackAssets models) {
            out.models = std::move(models);
            return AddEntities(vfs, *parsed, parsed->modelEntities, out);
        })
        .and_then([&] { return AddEntities(vfs, *parsed, parsed->animatedEntities, out); })
        .transform([&] { return std::move(out); });
}

Result<EffectAssets> LoadPowerupAssets(const assets::Vfs& vfs, assets::TrackLoader& loader) {
    return LoadEffectAssets(vfs, loader, kPowerupWorld);
}

Result<EffectModels> UploadEffectModels(render::Renderer& renderer, const EffectAssets& assets) {
    EffectModels models;
    for (const auto& [name, files] : assets.entities) {
        const auto geometry = assets.models.models.find(assets::ToUpperAscii(files.geometry));
        if (geometry == assets.models.models.end()) {
            return Fail(ErrorCode::NotFound, std::format("{}: no model {}", name, files.geometry));
        }
        Result<std::vector<render::MeshDraw>> draws =
            UploadModel(renderer, assets.models, files.geometry);
        if (!draws) {
            return std::unexpected(std::move(draws.error()));
        }
        models.emplace(name,
                       EffectModel{.draws = FollowJoints(*geometry->second, std::move(*draws)),
                                   .jointed = files.jointed ? &*files.jointed : nullptr,
                                   .placement = files.placement,
                                   .scroll = files.scroll,
                                   .rangeSquared = files.rangeSquared});
    }
    return models;
}

Result<EffectModelSet> LoadEffectModels(render::Renderer& renderer, const assets::Vfs& vfs,
                                        assets::TrackLoader& loader, std::string_view world) {
    EffectModelSet set;
    return LoadEffectAssets(vfs, loader, world)
        .and_then([&](EffectAssets loaded) {
            set.assets = std::make_unique<EffectAssets>(std::move(loaded));
            return UploadEffectModels(renderer, *set.assets);
        })
        .transform([&](EffectModels models) {
            set.models = std::move(models);
            return std::move(set);
        });
}

PowerupModelSet LoadPowerupModels(render::Renderer& renderer, const assets::Vfs& vfs,
                                  assets::TrackLoader& loader) {
    PowerupModelSet set;
    const std::vector<std::string> quadMaterials(PowerupQuadMaterials().begin(),
                                                 PowerupQuadMaterials().end());
    Result<void> loaded =
        LoadPowerupAssets(vfs, loader)
            .and_then([&](EffectAssets assets) {
                set.assets = std::make_unique<EffectAssets>(std::move(assets));
                return UploadEffectModels(renderer, *set.assets);
            })
            .and_then([&](EffectModels models) {
                set.models = std::move(models);
                return UploadMaterials(renderer, set.assets->models, quadMaterials);
            })
            .transform([&](assets::NameMap<render::Material> materials) {
                set.quadMaterials = std::move(materials);
            });
    if (!loaded) {
        log::Warn("power-up models: {}", loaded.error().message);
        return {};
    }
    return set;
}

}
