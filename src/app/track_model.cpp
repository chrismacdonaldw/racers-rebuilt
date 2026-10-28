#include "app/track_model.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>

#include "assets/game_path.hpp"

namespace rr::app {
namespace {

using assets::NameMap;

constexpr float kColorKeyCutoff = 0.5F;
constexpr float kBlendedCutoff = 1.0F / 255.0F;
constexpr float kChannelMax = 255.0F;

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
        const assets::GeometryAsset& geometry = *model->second.geometry;
        for (const assets::GeometryRange& range : geometry.ranges) {
            draws_.push_back({.mesh = model->second.mesh,
                              .firstIndex = range.firstIndex,
                              .indexCount = range.indexCount,
                              .material = MaterialFor(geometry, range),
                              .transform = transform});
        }
        Bounds& bounds = isSurface ? surface_ : everything_;
        for (const assets::GeometryVertex& vertex : geometry.vertices) {
            bounds.Add(Apply(transform, ToEngine(vertex.position)));
        }
    }

    [[nodiscard]] TrackModel Finish() && {
        return {.draws = std::move(draws_), .bounds = surface_.isEmpty ? everything_ : surface_};
    }

private:
    [[nodiscard]] render::Material MaterialFor(const assets::GeometryAsset& geometry,
                                               const assets::GeometryRange& range) const {
        if (range.material >= geometry.materials.size()) {
            return {};
        }
        const auto material =
            materials_.find(assets::ToUpperAscii(geometry.materials[range.material]));
        return material == materials_.end() ? render::Material{} : material->second;
    }

    const NameMap<UploadedModel>* models_;
    NameMap<render::Material> materials_;
    std::vector<render::MeshDraw> draws_;
    Bounds surface_;
    Bounds everything_;
};

}

Result<TrackModel> UploadTrack(render::Renderer& renderer, const assets::TrackAssets& track) {
    Result<NameMap<render::TextureId>> textures = UploadTextures(renderer, track.textures);
    Result<NameMap<UploadedModel>> models = textures.and_then(
        [&](const NameMap<render::TextureId>&) { return UploadModels(renderer, track.models); });
    if (!models) {
        return std::unexpected(std::move(models.error()));
    }
    NameMap<render::Material> materials;
    for (const auto& [name, material] : track.materials) {
        materials.emplace(name, ToRenderMaterial(material, *textures));
    }
    TrackBuilder builder(*models, std::move(materials));
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

}
