#include "app/sky_backdrop.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <utility>

#include "app/texture_scroll.hpp"
#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr std::uint32_t kSegments = 11;
constexpr float kRadius = 100.0F;
constexpr float kSegmentAngle = 2.0F * std::numbers::pi_v<float> / kSegments;
constexpr std::array kRingAngles{kSegmentAngle, 2.0F * kSegmentAngle,
                                 std::numbers::pi_v<float> / 2.0F};
constexpr std::uint32_t kBandIndices = kSegments * 6;
constexpr float kDomeDrop = 10.0F;
constexpr float kWorldLift = 30.0F;
constexpr float kChannelMax = 255.0F;

void AddRing(assets::GeometryAsset& dome, float angle, std::uint8_t shade) {
    for (std::uint32_t segment = 0; segment < kSegments; ++segment) {
        const float around = kSegmentAngle * static_cast<float>(segment);
        const Vector3 game{.x = kRadius * std::sin(angle) * std::cos(around),
                           .y = kRadius * std::sin(angle) * std::sin(around),
                           .z = kRadius * std::cos(angle)};
        dome.vertices.push_back({.position = ToEngine(game), .rgba = {shade, shade, shade, 255}});
    }
}

void AddBand(assets::GeometryAsset& dome, float upper, float lower) {
    const auto first = static_cast<std::uint32_t>(dome.vertices.size());
    AddRing(dome, upper, 255);
    AddRing(dome, lower, 0);
    for (std::uint32_t segment = 0; segment < kSegments; ++segment) {
        const std::uint32_t next = (segment + 1) % kSegments;
        const std::uint32_t top = first + segment;
        const std::uint32_t topNext = first + next;
        const std::uint32_t bottom = first + kSegments + segment;
        const std::uint32_t bottomNext = first + kSegments + next;
        dome.indices.insert(dome.indices.end(),
                            {top, bottom, bottomNext, top, bottomNext, topNext});
    }
}

render::Color ToColor(const assets::Rgb8& color) {
    return {.r = static_cast<float>(color[0]) / kChannelMax,
            .g = static_cast<float>(color[1]) / kChannelMax,
            .b = static_cast<float>(color[2]) / kChannelMax};
}

render::MeshDraw Band(render::MeshId dome, std::uint32_t band, const assets::Rgb8& upper,
                      const assets::Rgb8& lower, const Vector3& position) {
    const render::Color from = ToColor(lower);
    const render::Color to = ToColor(upper);
    return {.mesh = dome,
            .firstIndex = band * kBandIndices,
            .indexCount = kBandIndices,
            .material = {.vertexColors = {.scale = {.r = to.r - from.r,
                                                    .g = to.g - from.g,
                                                    .b = to.b - from.b},
                                          .offset = from}},
            .transform = {.position = position}};
}

}

assets::GeometryAsset SkyDomeGeometry() {
    assets::GeometryAsset dome;
    AddBand(dome, kRingAngles[0], kRingAngles[1]);
    AddBand(dome, kRingAngles[1], kRingAngles[2]);
    dome.ranges = {{.firstIndex = 0, .indexCount = kBandIndices},
                   {.firstIndex = kBandIndices, .indexCount = kBandIndices}};
    return dome;
}

Result<SkyBackdrop> SkyBackdrop::Upload(render::Renderer& renderer, std::vector<SkyModel> models) {
    return renderer.UploadMesh(SkyDomeGeometry()).transform([&](render::MeshId dome) {
        return SkyBackdrop(dome, std::move(models));
    });
}

void SkyBackdrop::Place(const sim::RaceSky& sky, const Vector3& camera, float shownMs,
                        std::vector<render::MeshDraw>& draws) const {
    draws.clear();
    if (!sky.ShowsDome()) {
        return;
    }
    const sim::SkyColors& colors = sky.Colors();
    const Vector3 dome = camera + ToEngine({.z = sky.HeightOffset() - kDomeDrop});
    draws.push_back(Band(dome_, 0, colors.top, colors.middle, dome));
    draws.push_back(Band(dome_, 1, colors.middle, colors.horizon, dome));
    if (!sky.ShowsWorld()) {
        return;
    }
    const Vector3 world = camera + ToEngine({.z = kWorldLift});
    for (const SkyModel& model : models_) {
        const render::UvOffset scrolled = ScrolledBy(model.scroll, shownMs);
        for (render::MeshDraw draw : model.draws) {
            draw.transform.position = world;
            draw.material.uvOffset = scrolled;
            draws.push_back(draw);
        }
    }
}

}
