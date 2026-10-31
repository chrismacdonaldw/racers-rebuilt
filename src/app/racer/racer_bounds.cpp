#include "app/racer/racer_bounds.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string_view>

#include "assets/game_path.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kWheelPart = "frwrd";
constexpr std::string_view kDriverPart = "default";

Result<const assets::GeometryAsset*> FindModel(const RacerAssets& racer, std::string_view name) {
    const auto model = racer.models.models.find(assets::ToUpperAscii(name));
    if (model == racer.models.models.end()) {
        return Fail(ErrorCode::NotFound, std::format("no model named {}", name));
    }
    return model->second.get();
}

Result<BoundingSphere> PartBounds(const RacerAssets& racer, const JointedAssets& jointed,
                                  std::string_view model, std::string_view part) {
    const auto& parts = jointed.animations.parts;
    const auto found = std::ranges::find(parts, part, &assets::AnimationPart::name);
    if (found == parts.end()) {
        return Fail(ErrorCode::NotFound, std::format("no animation part named {}", part));
    }
    return FindModel(racer, model).transform([&](const assets::GeometryAsset* geometry) {
        return BoundingSphere{.centre = found->bounds.centre * geometry->scale,
                              .radius = found->bounds.radius * geometry->scale};
    });
}

}

BoundingSphere ModelBounds(std::span<const assets::GeometryVertex> vertices) {
    if (vertices.empty()) {
        return {};
    }
    Vector3 low = vertices.front().position;
    Vector3 high = low;
    for (const assets::GeometryVertex& vertex : vertices) {
        low = {.x = std::min(low.x, vertex.position.x),
               .y = std::min(low.y, vertex.position.y),
               .z = std::min(low.z, vertex.position.z)};
        high = {.x = std::max(high.x, vertex.position.x),
                .y = std::max(high.y, vertex.position.y),
                .z = std::max(high.z, vertex.position.z)};
    }
    const Vector3 centre = (low + high) * 0.5F;
    float radius = 0.0F;
    for (const assets::GeometryVertex& vertex : vertices) {
        radius = std::max(radius, Length(vertex.position - centre));
    }
    return {.centre = centre, .radius = radius};
}

BoundingSphere GroupBounds(std::span<const BoundingSphere> members) {
    if (members.empty()) {
        return {};
    }
    Vector3 centre;
    for (const BoundingSphere& member : members) {
        centre += member.centre;
    }
    centre = centre * (1.0F / static_cast<float>(members.size()));
    float radius = 0.0F;
    for (const BoundingSphere& member : members) {
        radius = std::max(radius, member.radius + Length(member.centre - centre));
    }
    return {.centre = centre, .radius = radius};
}

Result<BoundingSphere> RacerBounds(const RacerAssets& racer) {
    const assets::RacerFiles& files = racer.files;
    std::array<BoundingSphere, 3> members;
    return FindModel(racer, files.champion.body.geometry)
        .and_then([&](const assets::GeometryAsset* body) {
            members[0] = ModelBounds(body->vertices);
            return PartBounds(racer, racer.wheels, files.chassis.wheels.model.geometry, kWheelPart);
        })
        .and_then([&](const BoundingSphere& wheels) {
            members[1] = wheels;
            return PartBounds(racer, racer.driver, files.driver.model.geometry, kDriverPart);
        })
        .transform([&](BoundingSphere driver) {
            driver.centre += files.chassis.seat;
            members[2] = driver;
            return GroupBounds(members);
        });
}

}
