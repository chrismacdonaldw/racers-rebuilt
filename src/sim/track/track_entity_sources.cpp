#include "sim/track/track_entity_sources.hpp"

#include <algorithm>
#include <format>
#include <map>
#include <span>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/geometry_database.hpp"
#include "assets/mirror.hpp"
#include "assets/world_database.hpp"
#include "sim/animation/part_clock.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr std::string_view kCommonFolder = "GAMEDATA/COMMON";
constexpr std::string_view kWorldExtension = ".WDB";

template <typename Parse>
auto ReadParsed(const assets::Vfs& vfs, const std::string& path, Parse parse) {
    return vfs.Read(path)
        .and_then([&](const assets::FileData& data) { return parse(data.Bytes()); })
        .transform_error([&](Error error) {
            error.message = std::format("{}: {}", path, error.message);
            return error;
        });
}

std::optional<std::size_t> StartPart(const assets::WorldEntity& entity,
                                     const assets::AnimationAsset& animation) {
    if (!entity.startPart) {
        return std::nullopt;
    }
    if (!entity.startPart->name.empty()) {
        const auto part = std::ranges::find(animation.parts, entity.startPart->name,
                                            &assets::AnimationPart::name);
        if (part == animation.parts.end()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(part - animation.parts.begin());
    }
    const auto index = static_cast<std::size_t>(entity.startPart->index);
    if (entity.startPart->index < 0 || index >= animation.parts.size()) {
        return std::nullopt;
    }
    return index;
}

struct ModelBounds {
    float scale = 1.0F;
    float radius = 0.0F;
};

Transform PlacementOf(const assets::WorldEntity& entity) {
    return {.position = entity.position,
            .rotation = ToQuaternion(OrthonormalBasis(entity.forward, entity.up))};
}

class EntityLoader {
public:
    EntityLoader(const assets::Vfs& vfs, std::string folder, std::vector<assets::VfsEntry> local,
                 std::vector<assets::VfsEntry> common, bool isMirrored)
        : vfs_(&vfs),
          folder_(std::move(folder)),
          local_(std::move(local)),
          common_(std::move(common)),
          isMirrored_(isMirrored) {}

    [[nodiscard]] std::vector<std::string> Worlds() const {
        std::vector<std::string> worlds;
        for (const assets::VfsEntry& entry : local_) {
            const std::string name = assets::ToUpperAscii(entry.name);
            if (entry.kind == assets::EntryKind::File && name.ends_with(kWorldExtension)) {
                worlds.push_back(assets::JoinGamePath(folder_, name));
            }
        }
        std::ranges::sort(worlds);
        return worlds;
    }

    Result<void> AddWorld(const std::string& path) {
        Result<assets::WorldAsset> world = ReadParsed(
            *vfs_, path,
            assets::MirroredIf(isMirrored_ && assets::IsMirroredWorld(path), assets::ParseWorld));
        if (!world) {
            return std::unexpected(std::move(world.error()));
        }
        for (const assets::WorldEntity& entity : world->modelEntities) {
            sources_.statics.push_back({.name = entity.name, .placement = PlacementOf(entity)});
        }
        for (const assets::WorldEntity& entity : world->animatedEntities) {
            if (Result<void> added = Add(*world, entity); !added) {
                return Fail(added.error().code,
                            std::format("{} {}: {}", path, entity.name, added.error().message));
            }
        }
        return {};
    }

    [[nodiscard]] TrackEntitySources Finish() && { return std::move(sources_); }

private:
    [[nodiscard]] Result<std::string> Resolve(std::string_view name,
                                              std::string_view extension) const {
        const std::string file = std::format("{}{}", assets::ToUpperAscii(name), extension);
        if (assets::HasFile(local_, file)) {
            return assets::JoinGamePath(folder_, file);
        }
        if (assets::HasFile(common_, file)) {
            return assets::JoinGamePath(kCommonFolder, file);
        }
        return Fail(ErrorCode::NotFound,
                    std::format("{} isn't in {} or {}", file, folder_, kCommonFolder));
    }

    template <typename Asset, typename Parse>
    Result<std::size_t> Intern(std::string_view name, std::string_view extension,
                               std::map<std::string, std::size_t>& indices,
                               std::vector<Asset>& assets, Parse parse) {
        const std::string key = assets::ToUpperAscii(name);
        if (const auto found = indices.find(key); found != indices.end()) {
            return found->second;
        }
        return Resolve(name, extension)
            .and_then([&](const std::string& path) {
                return ReadParsed(*vfs_, path, assets::MirroredIf(isMirrored_, parse));
            })
            .transform([&](Asset asset) {
                assets.push_back(std::move(asset));
                indices.emplace(key, assets.size() - 1);
                return assets.size() - 1;
            });
    }

    Result<ModelBounds> Bounds(const std::string& model) {
        if (model.empty()) {
            return ModelBounds{};
        }
        if (const auto found = modelBounds_.find(model); found != modelBounds_.end()) {
            return found->second;
        }
        return Resolve(model, ".GDB")
            .and_then([&](const std::string& path) {
                return ReadParsed(*vfs_, path, assets::ParseGeometry);
            })
            .transform([&](const assets::GeometryAsset& geometry) {
                ModelBounds bounds{.scale = geometry.scale};
                for (const assets::GeometryVertex& vertex : geometry.vertices) {
                    bounds.radius = std::max(bounds.radius, Length(vertex.position));
                }
                modelBounds_.emplace(model, bounds);
                return bounds;
            });
    }

    Result<void> Add(const assets::WorldAsset& world, const assets::WorldEntity& entity) {
        if (entity.lods.empty() || entity.lods.front().sceneNode == assets::kNoIndex ||
            entity.lods.front().modelPart == assets::kNoIndex) {
            return {};
        }
        const assets::WorldLod& lod = entity.lods.front();
        const auto nameAt = [](const std::vector<std::string>& names, std::int32_t index) {
            return index == assets::kNoIndex ? std::string{}
                                             : names.at(static_cast<std::size_t>(index));
        };
        TrackEntity placed{.name = entity.name,
                           .model = nameAt(world.models, lod.model),
                           .placement = PlacementOf(entity)};
        Result<ModelBounds> bounds = Bounds(placed.model);
        Result<std::size_t> skeleton = bounds.and_then([&](const ModelBounds&) {
            return Intern(nameAt(world.sceneNodes, lod.sceneNode), ".SDB", skeletonIndices_,
                          sources_.skeletons, assets::ParseSkeleton);
        });
        Result<std::size_t> animation = skeleton.and_then([&](std::size_t) {
            return Intern(nameAt(world.modelParts, lod.modelPart), ".ADB", animationIndices_,
                          sources_.animations, assets::ParseAnimations);
        });
        Result<void> checked = animation.and_then([&](std::size_t index) {
            return CheckPartTracks(sources_.animations[index],
                                   sources_.skeletons[*skeleton].nodes.size());
        });
        if (!checked) {
            return std::unexpected(std::move(checked.error()));
        }
        placed.skeleton = *skeleton;
        placed.animation = *animation;
        placed.startPart = StartPart(entity, sources_.animations[*animation]);
        placed.scale = bounds->scale * entity.scale;
        placed.radius = bounds->radius * entity.scale;
        sources_.entities.push_back(std::move(placed));
        return {};
    }

    const assets::Vfs* vfs_;
    std::string folder_;
    std::vector<assets::VfsEntry> local_;
    std::vector<assets::VfsEntry> common_;
    bool isMirrored_ = false;
    std::map<std::string, std::size_t> skeletonIndices_;
    std::map<std::string, std::size_t> animationIndices_;
    std::map<std::string, ModelBounds> modelBounds_;
    TrackEntitySources sources_;
};

}

Result<TrackEntitySources> LoadTrackEntities(const assets::Vfs& vfs, std::string_view track,
                                             bool isMirrored) {
    const std::string folder = std::format("GAMEDATA/{}", assets::ToUpperAscii(track));
    Result<std::vector<assets::VfsEntry>> local = vfs.List(folder);
    if (!local) {
        return std::unexpected(std::move(local.error()));
    }
    EntityLoader loader(vfs, folder, std::move(*local),
                        vfs.List(kCommonFolder).value_or(std::vector<assets::VfsEntry>{}),
                        isMirrored);
    for (const std::string& world : loader.Worlds()) {
        if (Result<void> added = loader.AddWorld(world); !added) {
            return std::unexpected(std::move(added.error()));
        }
    }
    return std::move(loader).Finish();
}

}
