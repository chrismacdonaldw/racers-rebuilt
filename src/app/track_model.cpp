#include "app/track_model.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
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

JointedModel ToJointed(const UploadedModel& model, const NameMap<render::Material>& materials) {
    JointedModel jointed;
    AppendDraws(jointed.draws, model, materials, {});
    for (const assets::GeometryRange& range : model.geometry->ranges) {
        jointed.joints.push_back(range.joint);
        jointed.slots.push_back(range.material);
    }
    return jointed;
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
        const auto model = ModelOf(world, entity);
        if (model == models_->end()) {
            return;
        }
        const Transform transform = PlacementToEngine(entity.position, entity.forward, entity.up);
        const std::size_t first = draws_.size();
        AppendDraws(draws_, model->second, materials_, transform);
        for (const assets::GeometryRange& range : model->second.geometry->ranges) {
            slots_.push_back(range.material);
        }
        named_.push_back(
            {.entity = entity.name, .span = {.first = first, .count = draws_.size() - first}});
        AddBounds(model->second, transform, isSurface ? surface_ : everything_);
    }

    void Keep(const assets::WorldAsset& world, const assets::WorldEntity& entity) {
        const auto model = ModelOf(world, entity);
        if (model == models_->end()) {
            return;
        }
        AddBounds(model->second, PlacementToEngine(entity.position, entity.forward, entity.up),
                  everything_);
        if (!animated_.contains(model->first)) {
            animated_.emplace(model->first, ToJointed(model->second, materials_));
        }
    }

    [[nodiscard]] TrackModel Finish() && {
        for (render::MeshDraw& draw : draws_) {
            draw.isStatic = true;
        }
        return {.draws = std::move(draws_),
                .drawSlots = std::move(slots_),
                .entityDraws = std::move(named_),
                .animatedModels = std::move(animated_),
                .materials = std::move(materials_),
                .bounds = surface_.isEmpty ? everything_ : surface_};
    }

private:
    [[nodiscard]] NameMap<UploadedModel>::const_iterator ModelOf(
        const assets::WorldAsset& world, const assets::WorldEntity& entity) const {
        if (entity.lods.empty() || entity.lods.front().model == assets::kNoIndex) {
            return models_->end();
        }
        const std::string& name =
            world.models.at(static_cast<std::size_t>(entity.lods.front().model));
        return models_->find(assets::ToUpperAscii(name));
    }

    static void AddBounds(const UploadedModel& model, const Transform& transform, Bounds& bounds) {
        for (const assets::GeometryVertex& vertex : model.geometry->vertices) {
            bounds.Add(Apply(transform, ToEngine(vertex.position)));
        }
    }

    const NameMap<UploadedModel>* models_;
    NameMap<render::Material> materials_;
    std::vector<render::MeshDraw> draws_;
    std::vector<std::uint32_t> slots_;
    std::vector<NamedDraws> named_;
    NameMap<JointedModel> animated_;
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
            .filter = material.filter == assets::TextureFilter::Point
                          ? render::TextureFilter::Point
                          : render::TextureFilter::Linear,
            .address = material.address == assets::TextureAddress::Clamp
                           ? render::TextureAddress::Clamp
                           : render::TextureAddress::Repeat,
            .tint = tint,
            .ambient = ToColor(material.ambient),
            .alphaCutoff = blend == render::BlendMode::Opaque ? kColorKeyCutoff : kBlendedCutoff,
            .blend = blend,
            .modulates = !isTextured || material.textureMode == assets::TextureMode::Modulate ||
                         material.transparency.has_value(),
            .shading = material.shading == assets::Shading::Flat ? render::Shading::Flat
                                                                 : render::Shading::Gouraud};
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
            builder.Keep(*world, entity);
        }
        for (const assets::WorldEntity& entity : world->collidableEntities) {
            builder.Place(*world, entity, true);
        }
    }
    TrackModel model = std::move(builder).Finish();
    model.textures = std::move(*textures);
    return model;
}

Result<std::vector<render::MeshDraw>> UploadModel(render::Renderer& renderer,
                                                  const assets::TrackAssets& assets,
                                                  std::string_view name) {
    const auto model = assets.models.find(assets::ToUpperAscii(name));
    if (model == assets.models.end() || model->second->indices.empty()) {
        return Fail(ErrorCode::NotFound, std::format("no model named {}", name));
    }
    Result<NameMap<render::Material>> materials =
        UploadMaterials(renderer, assets, model->second->materials);
    Result<NameMap<UploadedModel>> uploaded = materials.and_then(
        [&](const NameMap<render::Material>&) { return UploadModels(renderer, {*model}); });
    if (!uploaded) {
        return std::unexpected(std::move(uploaded.error()));
    }
    std::vector<render::MeshDraw> draws;
    AppendDraws(draws, uploaded->begin()->second, *materials, {});
    return draws;
}

Result<JointedModel> UploadJointedModel(render::Renderer& renderer,
                                        const assets::GeometryAsset& geometry,
                                        const NameMap<render::Material>& materials) {
    return renderer.UploadMesh(ToEngineGeometry(geometry)).transform([&](render::MeshId mesh) {
        return ToJointed({.mesh = mesh, .geometry = &geometry}, materials);
    });
}

Result<NameMap<render::Material>> UploadMaterials(render::Renderer& renderer,
                                                  const assets::TrackAssets& assets,
                                                  std::span<const std::string> names) {
    NameMap<assets::MaterialAsset> materials;
    NameMap<assets::AssetCache<assets::ImageAsset>::Handle> images;
    for (const std::string& name : names) {
        const auto material = assets.materials.find(assets::ToUpperAscii(name));
        if (material == assets.materials.end()) {
            continue;
        }
        materials.insert(*material);
        const auto image = assets.textures.find(assets::ToUpperAscii(material->second.texture));
        if (image != assets.textures.end()) {
            images.insert(*image);
        }
    }
    return UploadTextures(renderer, images)
        .transform([&](const NameMap<render::TextureId>& textures) {
            return ToRenderMaterials(materials, textures);
        });
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
