#include "app/hazard/crane_shadow.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <variant>

#include "app/particle_assets.hpp"
#include "app/track_model.hpp"
#include "app/track_space.hpp"
#include "assets/model_files.hpp"
#include "core/log.hpp"
#include "core/quaternion.hpp"
#include "sim/hazard/crane.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kShadowMaterial = "CRNESHD";
constexpr Vector3 kGameForward{.x = 1.0F};
constexpr Vector3 kGameUp{.z = 1.0F};
constexpr float kShadowSize = 13.0F;

Result<std::optional<QuadLook>> UploadShadowLook(render::Renderer& renderer,
                                                 assets::TrackLoader& loader,
                                                 const ParticleFiles& files) {
    if (!files.materials) {
        return std::nullopt;
    }
    const std::array models{
        assets::ModelFiles{.materials = *files.materials, .textures = *files.materials}};
    return loader.LoadModels(files.folder, models)
        .and_then([&](const assets::TrackAssets& assets) {
            const std::array names{std::string(kShadowMaterial)};
            return UploadMaterials(renderer, assets, names);
        })
        .transform([](const assets::NameMap<render::Material>& materials) {
            const auto found = materials.find(kShadowMaterial);
            return found == materials.end() ? std::nullopt : std::optional(LookOf(found->second));
        });
}

bool IsSeen(const Vector3& ball, std::span<const Vector3> cameras, float rangeSquared) {
    return cameras.empty() || std::ranges::any_of(cameras, [&](const Vector3& camera) {
               const Vector3 offset = ball - camera;
               return Dot(offset, offset) < rangeSquared;
           });
}

}

std::optional<QuadLook> LoadCraneShadowLook(render::Renderer& renderer, const assets::Vfs& vfs,
                                            assets::TrackLoader& loader, std::string_view track) {
    Result<std::optional<QuadLook>> look =
        TrackParticleFiles(vfs, track).and_then([&](const ParticleFiles& files) {
            return UploadShadowLook(renderer, loader, files);
        });
    if (!look) {
        log::Warn("crane shadow: {}", look.error().message);
        return std::nullopt;
    }
    return *look;
}

void CraneShadow::Append(const sim::Hazards& hazards, std::span<const sim::EntityClock> clocks,
                         std::span<const Vector3> cameras, GroundDecals& ground,
                         std::vector<render::QuadDraw>& out) const {
    if (!look_) {
        return;
    }
    for (const sim::HazardSlot& slot : hazards.Slots()) {
        const auto* crane = std::get_if<sim::Crane>(&slot.state);
        if (crane != nullptr && crane->entity && *crane->entity < clocks.size() &&
            slot.phase == sim::HazardPhase::Active) {
            AppendUnder(*crane->entity, clocks[*crane->entity], cameras, ground, out);
        }
    }
}

void CraneShadow::AppendUnder(std::size_t crane, const sim::EntityClock& clock,
                              std::span<const Vector3> cameras, GroundDecals& ground,
                              std::vector<render::QuadDraw>& out) const {
    const sim::TrackEntity& entity = sources_->entities[crane];
    const Vector3 ball = sim::NodeWorld(*sources_, crane, clock, sim::kCraneBallNode).position;
    if (!IsSeen(ToEngine(ball), cameras, entity.drawRangeSquared)) {
        return;
    }
    ground.Append(*look_,
                  {.centre = ball - (kGameUp * kDecalCastAbove),
                   .forward = Rotate(entity.placement.rotation, kGameForward),
                   .length = kShadowSize,
                   .width = kShadowSize},
                  out);
}

}
