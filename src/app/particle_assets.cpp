#include "app/particle_assets.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <utility>

#include "app/quad_shapes.hpp"
#include "app/track_model.hpp"
#include "app/track_space.hpp"
#include "assets/game_path.hpp"
#include "assets/model_files.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kCommonFolder = "GAMEDATA/COMMON";
constexpr std::string_view kCommonFiles = "EMITTER";
constexpr std::string_view kTrackFilePrefix = "EMIT";
constexpr std::string_view kAnimationExtension = ".MAB";

template <typename Parse>
auto ReadParsed(const assets::Vfs& vfs, const std::string& path, Parse parse) {
    return vfs.Read(path)
        .and_then([&](const assets::FileData& data) { return parse(data.Bytes()); })
        .transform_error([&](Error error) {
            error.message = std::format("{}: {}", path, error.message);
            return error;
        });
}

std::optional<render::ParticleLook> LookOf(std::string_view name,
                                           const assets::NameMap<assets::MaterialAsset>& materials,
                                           const assets::NameMap<render::TextureId>& textures) {
    const auto found = materials.find(assets::ToUpperAscii(name));
    if (found == materials.end()) {
        return std::nullopt;
    }
    const render::Material material = ToRenderMaterial(found->second, textures);
    return render::ParticleLook{
        .texture = material.texture,
        .tint = material.modulates ? material.tint : render::Color{.a = material.tint.a},
        .blend =
            material.blend == render::BlendMode::Opaque ? render::BlendMode::Alpha : material.blend,
        .sampling = SamplingOf(material)};
}

std::string SharedFiles(std::span<const assets::VfsEntry> entries, std::string_view track) {
    std::string named = std::format("{}{}", kTrackFilePrefix, assets::RaceTrackStem(track));
    if (assets::HasFile(entries, named + std::string(kAnimationExtension))) {
        return named;
    }
    const auto renamed = std::ranges::find_if(entries, [](const assets::VfsEntry& entry) {
        return entry.name.starts_with(kTrackFilePrefix) &&
               entry.name.ends_with(kAnimationExtension);
    });
    return renamed == entries.end()
               ? named
               : renamed->name.substr(0, renamed->name.size() - kAnimationExtension.size());
}

assets::EmitterAsset ToEngineEmitter(assets::EmitterAsset emitter) {
    emitter.acceleration = ToEngine(emitter.acceleration);
    for (Vector3& point : emitter.points) {
        point = ToEngine(point);
    }
    return emitter;
}

}

ParticleFiles CommonParticleFiles() {
    return {.folder = std::string(kCommonFolder),
            .emitters = std::string(kCommonFiles),
            .animation = std::string(kCommonFiles),
            .materials = std::string(kCommonFiles)};
}

Result<ParticleFiles> TrackParticleFiles(const assets::Vfs& vfs, std::string_view track) {
    const std::string folder = std::format("GAMEDATA/{}", track);
    return vfs.List(folder).transform([&](const std::vector<assets::VfsEntry>& entries) {
        const std::string shared = SharedFiles(entries, track);
        const bool hasMaterials = assets::HasFile(entries, std::format("{}.MDB", shared));
        return ParticleFiles{.folder = folder,
                             .emitters = std::string(track),
                             .animation = shared,
                             .materials = hasMaterials ? std::optional(shared) : std::nullopt};
    });
}

Result<ParticleSources> ReadParticleSources(const assets::Vfs& vfs, const ParticleFiles& files) {
    return ReadParsed(vfs, std::format("{}/{}.EMB", files.folder, files.emitters),
                      assets::ParseEmitterLibrary)
        .and_then([&](std::vector<assets::EmitterAsset> emitters) {
            return ReadParsed(vfs, std::format("{}/{}.MAB", files.folder, files.animation),
                              assets::ParseMaterialAnimation)
                .transform([&](assets::MaterialAnimationAsset animation) {
                    return ParticleSources{.emitters = std::move(emitters),
                                           .animation = std::move(animation)};
                });
        });
}

render::ParticleBankSetup ToParticleSetup(ParticleSources sources,
                                          const assets::NameMap<assets::MaterialAsset>& materials,
                                          const assets::NameMap<render::TextureId>& textures,
                                          std::size_t slotCount) {
    render::ParticleBankSetup setup{.animation = std::move(sources.animation),
                                    .slotCount = slotCount};
    for (assets::EmitterAsset& emitter : sources.emitters) {
        setup.emitterLooks.push_back(
            emitter.material ? LookOf(*emitter.material, materials, textures) : std::nullopt);
        setup.emitters.push_back(ToEngineEmitter(std::move(emitter)));
    }
    for (const assets::MaterialKey& key : setup.animation.keys) {
        setup.keyLooks.push_back(LookOf(key.material, materials, textures));
    }
    return setup;
}

Result<render::ParticleBankSetup> LoadParticleBank(render::Renderer& renderer,
                                                   const assets::Vfs& vfs,
                                                   assets::TrackLoader& loader,
                                                   const ParticleFiles& files,
                                                   std::size_t slotCount) {
    return ReadParticleSources(vfs, files)
        .and_then([&](ParticleSources sources) -> Result<render::ParticleBankSetup> {
            if (!files.materials) {
                return ToParticleSetup(std::move(sources), {}, {}, slotCount);
            }
            const std::array models{
                assets::ModelFiles{.materials = *files.materials, .textures = *files.materials}};
            return loader.LoadModels(files.folder, models)
                .and_then([&](const assets::TrackAssets& assets) {
                    return UploadTextures(renderer, assets.textures)
                        .transform([&](const assets::NameMap<render::TextureId>& textures) {
                            return ToParticleSetup(std::move(sources), assets.materials, textures,
                                                   slotCount);
                        });
                });
        });
}

}
