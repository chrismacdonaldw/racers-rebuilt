#include "app/track_model.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"

namespace rr::app {
namespace {

using assets::NameMap;

constexpr float kColorKeyCutoff = 0.5F;
constexpr float kBlendedCutoff = 1.0F / 255.0F;
constexpr float kChannelMax = 255.0F;
constexpr float kNearPlaneRadii = 0.001F;
constexpr float kFarPlaneRadii = 8.0F;
constexpr std::string_view kStandInCarFolder = "MENUDATA/CIRCUIT1";
constexpr std::string_view kStandInCarModel = "CR_CAR";

struct UploadedModel {
    render::MeshId mesh{};
    const assets::GeometryAsset* geometry = nullptr;
};

template <typename T>
Result<T> WithName(Result<T> result, const std::string& name) {
    if (!result) {
        return Fail(result.error().code, std::format("{}: {}", name, result.error().message));
    }
    return result;
}

assets::GeometryAsset ToEngineGeometry(const assets::GeometryAsset& geometry) {
    assets::GeometryAsset converted{.vertices = geometry.vertices, .indices = geometry.indices};
    for (assets::GeometryVertex& vertex : converted.vertices) {
        vertex.position = ToEngine(vertex.position);
        vertex.normal = ToEngine(vertex.normal);
    }
    return converted;
}

Result<NameMap<UploadedModel>> UploadModels(
    render::Renderer& renderer,
    const NameMap<assets::AssetCache<assets::GeometryAsset>::Handle>& models) {
    NameMap<UploadedModel> uploaded;
    for (const auto& [name, geometry] : models) {
        if (geometry->indices.empty()) {
            continue;
        }
        Result<render::MeshId> mesh =
            WithName(renderer.UploadMesh(ToEngineGeometry(*geometry)), name);
        if (!mesh) {
            return std::unexpected(std::move(mesh.error()));
        }
        uploaded.emplace(name, UploadedModel{.mesh = *mesh, .geometry = geometry.get()});
    }
    return uploaded;
}

float ToUnit(std::uint8_t channel) {
    return static_cast<float>(channel) / kChannelMax;
}

render::Color ToColor(const Rgba8& color) {
    return {.r = ToUnit(color[0]), .g = ToUnit(color[1]), .b = ToUnit(color[2])};
}

render::BlendMode BlendModeOf(const assets::MaterialAsset& material) {
    if (material.blend) {
        return material.blend->destination == assets::BlendFactor::One ? render::BlendMode::Additive
                                                                       : render::BlendMode::Alpha;
    }
    return material.transparency ? render::BlendMode::Alpha : render::BlendMode::Opaque;
}

render::Material MaterialFor(const NameMap<render::Material>& materials,
                             const assets::GeometryAsset& geometry,
                             const assets::GeometryRange& range) {
    if (range.material >= geometry.materials.size()) {
        return {};
    }
    const auto material = materials.find(assets::ToUpperAscii(geometry.materials[range.material]));
    return material == materials.end() ? render::Material{} : material->second;
}

void AppendDraws(std::vector<render::MeshDraw>& draws, const UploadedModel& model,
                 const NameMap<render::Material>& materials, const Transform& transform) {
    for (const assets::GeometryRange& range : model.geometry->ranges) {
        draws.push_back({.mesh = model.mesh,
                         .firstIndex = range.firstIndex,
                         .indexCount = range.indexCount,
                         .material = MaterialFor(materials, *model.geometry, range),
                         .transform = transform});
    }
}

NameMap<render::Material> ToRenderMaterials(const NameMap<assets::MaterialAsset>& materials,
                                            const NameMap<render::TextureId>& textures) {
    NameMap<render::Material> converted;
    for (const auto& [name, material] : materials) {
        converted.emplace(name, ToRenderMaterial(material, textures));
    }
    return converted;
}

class TrackBuilder {
public:
    TrackBuilder(const NameMap<UploadedModel>& models, NameMap<render::Material> materials)
        : models_(&models), materials_(std::move(materials)) {}

    void Place(const assets::WorldAsset& world, const assets::WorldEntity& entity, bool isSurface) {
        if (entity.lods.empty() || entity.lods.front().model == assets::kNoIndex) {
            return;
        }
        const std::string& name =
            world.models.at(static_cast<std::size_t>(entity.lods.front().model));
        const auto model = models_->find(assets::ToUpperAscii(name));
        if (model == models_->end()) {
            return;
        }
        const Transform transform = PlacementToEngine(entity.position, entity.forward, entity.up);
        AppendDraws(draws_, model->second, materials_, transform);
        Bounds& bounds = isSurface ? surface_ : everything_;
        for (const assets::GeometryVertex& vertex : model->second.geometry->vertices) {
            bounds.Add(Apply(transform, ToEngine(vertex.position)));
        }
    }

    [[nodiscard]] TrackModel Finish() && {
        return {.draws = std::move(draws_), .bounds = surface_.isEmpty ? everything_ : surface_};
    }

private:
    const NameMap<UploadedModel>* models_;
    NameMap<render::Material> materials_;
    std::vector<render::MeshDraw> draws_;
    Bounds surface_;
    Bounds everything_;
};

}

Result<NameMap<render::TextureId>> UploadTextures(
    render::Renderer& renderer,
    const NameMap<assets::AssetCache<assets::ImageAsset>::Handle>& images) {
    NameMap<render::TextureId> textures;
    for (const auto& [name, image] : images) {
        Result<render::TextureId> texture = WithName(renderer.UploadTexture(*image), name);
        if (!texture) {
            return std::unexpected(std::move(texture.error()));
        }
        textures.emplace(name, *texture);
    }
    return textures;
}

render::Material ToRenderMaterial(const assets::MaterialAsset& material,
                                  const NameMap<render::TextureId>& textures) {
    const auto texture = textures.find(assets::ToUpperAscii(material.texture));
    const bool isTextured = texture != textures.end();
    render::Color tint = ToColor(material.diffuse);
    tint.a = material.transparency ? ToUnit(*material.transparency) : 1.0F;
    const render::BlendMode blend = BlendModeOf(material);
    return {.texture = isTextured ? texture->second : render::kWhiteTexture,
            .tint = tint,
            .ambient = ToColor(material.ambient),
            .alphaCutoff = blend == render::BlendMode::Opaque ? kColorKeyCutoff : kBlendedCutoff,
            .blend = blend,
            .modulates = !isTextured || material.textureMode == assets::TextureMode::Modulate ||
                         material.transparency.has_value()};
}

Result<TrackModel> UploadTrack(render::Renderer& renderer, const assets::TrackAssets& track) {
    Result<NameMap<render::TextureId>> textures = UploadTextures(renderer, track.textures);
    Result<NameMap<UploadedModel>> models = textures.and_then(
        [&](const NameMap<render::TextureId>&) { return UploadModels(renderer, track.models); });
    if (!models) {
        return std::unexpected(std::move(models.error()));
    }
    TrackBuilder builder(*models, ToRenderMaterials(track.materials, *textures));
    for (const assets::AssetCache<assets::WorldAsset>::Handle& world : track.worlds) {
        for (const assets::WorldEntity& entity : world->modelEntities) {
            builder.Place(*world, entity, false);
        }
        for (const assets::WorldEntity& entity : world->animatedEntities) {
            builder.Place(*world, entity, false);
        }
        for (const assets::WorldEntity& entity : world->collidableEntities) {
            builder.Place(*world, entity, true);
        }
    }
    return std::move(builder).Finish();
}

Result<std::vector<render::MeshDraw>> UploadModel(render::Renderer& renderer,
                                                  const assets::TrackAssets& assets,
                                                  std::string_view name) {
    const auto model = assets.models.find(assets::ToUpperAscii(name));
    if (model == assets.models.end() || model->second->indices.empty()) {
        return Fail(ErrorCode::NotFound, std::format("no model named {}", name));
    }
    NameMap<assets::MaterialAsset> materials;
    NameMap<assets::AssetCache<assets::ImageAsset>::Handle> images;
    for (const std::string& materialName : model->second->materials) {
        const auto material = assets.materials.find(assets::ToUpperAscii(materialName));
        if (material == assets.materials.end()) {
            continue;
        }
        materials.insert(*material);
        const auto image = assets.textures.find(assets::ToUpperAscii(material->second.texture));
        if (image != assets.textures.end()) {
            images.insert(*image);
        }
    }
    Result<NameMap<render::TextureId>> textures = UploadTextures(renderer, images);
    Result<NameMap<UploadedModel>> uploaded = textures.and_then(
        [&](const NameMap<render::TextureId>&) { return UploadModels(renderer, {*model}); });
    if (!uploaded) {
        return std::unexpected(std::move(uploaded.error()));
    }
    std::vector<render::MeshDraw> draws;
    AppendDraws(draws, uploaded->begin()->second, ToRenderMaterials(materials, *textures), {});
    return draws;
}

Result<std::vector<render::MeshDraw>> UploadStandInCar(render::Renderer& renderer,
                                                       assets::TrackLoader& loader) {
    return loader.Load(kStandInCarFolder).and_then([&](const assets::TrackAssets& assets) {
        return UploadModel(renderer, assets, kStandInCarModel);
    });
}

render::Camera TrackCamera(const Bounds& bounds) {
    const Vector3 size{.x = bounds.max.x - bounds.min.x,
                       .y = bounds.max.y - bounds.min.y,
                       .z = bounds.max.z - bounds.min.z};
    const float radius = Length(size) / 2.0F;
    render::Camera camera;
    camera.nearPlane = radius * kNearPlaneRadii;
    camera.farPlane = radius * kFarPlaneRadii;
    return camera;
}

}
