#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/decal_surface.hpp"
#include "app/track_space.hpp"
#include "assets/track_loader.hpp"
#include "assets/world_database.hpp"
#include "core/result.hpp"
#include "render/camera.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// Draws at the model's origin. Draw `i` hangs from node `joints[i]` and fills slot `slots[i]`.
struct JointedModel {
    std::vector<render::MeshDraw> draws;
    std::vector<std::uint16_t> joints;
    std::vector<std::uint32_t> slots;
};

struct DrawSpan {
    std::size_t first = 0;
    std::size_t count = 0;
};

struct NamedDraws {
    std::string entity;
    DrawSpan span;
};

/// A sky world model. Its draws hold the entity's rotation; the race moves them to the camera.
struct SkyModel {
    std::vector<render::MeshDraw> draws;
    assets::TextureScroll scroll;
};

/// `draws` leaves out the animated entities, and `entityDraws` says which draws each entity owns.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct TrackModel {
    std::vector<render::MeshDraw> draws;
    std::vector<std::uint32_t> drawSlots;
    std::vector<NamedDraws> entityDraws;
    assets::NameMap<JointedModel> animatedModels;
    std::vector<SkyModel> skyModels;
    assets::NameMap<render::Material> materials;
    Bounds bounds;
    assets::NameMap<render::TextureId> textures;
    DecalSurface decalSurface;
};

[[nodiscard]] Result<assets::NameMap<render::TextureId>> UploadTextures(
    render::Renderer& renderer,
    const assets::NameMap<assets::AssetCache<assets::ImageAsset>::Handle>& images);

[[nodiscard]] render::Material ToRenderMaterial(const assets::MaterialAsset& material,
                                                const assets::NameMap<render::TextureId>& textures);

/// `bounds` covers the collidable track surface, or every model when there is none.
[[nodiscard]] Result<TrackModel> UploadTrack(render::Renderer& renderer,
                                             const assets::TrackAssets& track);

/// At the origin, for the caller to move.
[[nodiscard]] Result<std::vector<render::MeshDraw>> UploadModel(render::Renderer& renderer,
                                                                const assets::TrackAssets& assets,
                                                                std::string_view name);

/// As `UploadTrack` keeps an animated entity's model. `materials` is keyed by uppercase name.
[[nodiscard]] Result<JointedModel> UploadJointedModel(
    render::Renderer& renderer, const assets::GeometryAsset& geometry,
    const assets::NameMap<render::Material>& materials);

/// Keyed by uppercase name. Names missing from `assets` are left out.
[[nodiscard]] Result<assets::NameMap<render::Material>> UploadMaterials(
    render::Renderer& renderer, const assets::TrackAssets& assets,
    std::span<const std::string> names);

/// A menu car that stands in for racer models, facing game +Y.
[[nodiscard]] Result<std::vector<render::MeshDraw>> UploadStandInCar(render::Renderer& renderer,
                                                                     assets::TrackLoader& loader);

/// Clip planes that fit a track of this size.
[[nodiscard]] render::Camera TrackCamera(const Bounds& bounds);

}
