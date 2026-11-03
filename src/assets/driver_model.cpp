#include "assets/driver_model.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <string_view>
#include <utility>
#include <vector>

namespace rr::assets {
namespace {

constexpr std::string_view kRaceBody = "leg_box";
constexpr std::string_view kFaceMaterial = "face";
constexpr std::string_view kTorsoMaterial = "torso";
constexpr std::string_view kLegsMaterial = "legs";
constexpr std::size_t kLegVariantStride = 2;

template <typename Part>
Result<const Part*> PartAt(const std::vector<Part>& parts, std::uint8_t index,
                           std::string_view slot) {
    if (index >= parts.size()) {
        return Fail(ErrorCode::InvalidData, std::format("{} {} is past the {} the catalogue lists",
                                                        slot, index, parts.size()));
    }
    return &parts[index];
}

struct ChosenParts {
    const DriverPart* face = nullptr;
    const DriverPart* torso = nullptr;
    const DriverPart* legs = nullptr;
    const std::string* headPart = nullptr;
};

void FitIndex(std::uint8_t& index, std::size_t count) {
    if (index >= count) {
        index = 0;
    }
}

Result<ChosenParts> ChooseParts(const DriverPartCatalog& catalog, const DriverLook& worn) {
    const DriverLook look = FitLook(catalog, worn);
    ChosenParts chosen;
    return PartAt(catalog.faces.parts, look.face, "face")
        .and_then([&](const DriverPart* face) {
            chosen.face = face;
            return PartAt(catalog.torsos.parts, look.torso, "torso");
        })
        .and_then([&](const DriverPart* torso) {
            chosen.torso = torso;
            return PartAt(catalog.legs.parts, look.legs, "legs");
        })
        .and_then([&](const DriverPart* legs) {
            chosen.legs = legs;
            return PartAt(catalog.headParts, look.hat, "hat");
        })
        .transform([&](const std::string* headPart) {
            chosen.headPart = headPart;
            return chosen;
        });
}

Result<std::uint16_t> FaceJoint(const GeometryAsset& body) {
    const auto material = std::ranges::find(body.materials, kFaceMaterial);
    const auto faceIndex = static_cast<std::uint32_t>(material - body.materials.begin());
    const auto range = std::ranges::find(body.ranges, faceIndex, &GeometryRange::material);
    if (material == body.materials.end() || range == body.ranges.end()) {
        return Fail(ErrorCode::InvalidData, "the driver body has no face");
    }
    return range->joint;
}

GeometryAsset MergeHead(GeometryAsset body, const GeometryAsset& head, std::uint16_t joint) {
    const auto face = static_cast<std::uint32_t>(std::ranges::find(body.materials, kFaceMaterial) -
                                                 body.materials.begin());
    std::erase_if(body.ranges,
                  [face](const GeometryRange& range) { return range.material == face; });
    const auto firstVertex = static_cast<std::uint32_t>(body.vertices.size());
    const auto firstIndex = static_cast<std::uint32_t>(body.indices.size());
    for (GeometryVertex vertex : head.vertices) {
        vertex.joint = joint;
        body.vertices.push_back(vertex);
    }
    for (const std::uint32_t index : head.indices) {
        body.indices.push_back(firstVertex + index);
    }
    for (const GeometryRange& range : head.ranges) {
        body.ranges.push_back({
            .firstIndex = firstIndex + range.firstIndex,
            .indexCount = range.indexCount,
            .material = FindOrAddMaterial(body.materials, head.materials.at(range.material)),
            .joint = joint,
        });
    }
    return body;
}

}

DriverLook FitLook(const DriverPartCatalog& catalog, DriverLook look) {
    FitIndex(look.face, catalog.faces.parts.size());
    FitIndex(look.hat, catalog.headParts.size());
    FitIndex(look.torso, catalog.torsos.parts.size());
    FitIndex(look.legs, catalog.legs.parts.size());
    return look;
}

Result<std::string> DriverBodyName(const DriverPartCatalog& catalog, const DriverLook& look,
                                   DriverDetail detail) {
    if (detail == DriverDetail::Race) {
        return std::string(kRaceBody);
    }
    return ChooseParts(catalog, look).and_then([&](const ChosenParts& chosen) {
        const auto body = static_cast<std::size_t>(chosen.torso->variant) +
                          (kLegVariantStride * static_cast<std::size_t>(chosen.legs->variant));
        if (body >= catalog.bodies.size()) {
            return Result<std::string>(
                Fail(ErrorCode::InvalidData, std::format("body {} is past the catalogue's", body)));
        }
        return Result<std::string>(catalog.bodies[body]);
    });
}

Result<GeometryAsset> BuildDriverGeometry(const DriverPartCatalog& catalog,
                                          const PartLibrary& heads, const GeometryAsset& body,
                                          const DriverLook& look, DriverDetail detail) {
    const Result<ChosenParts> chosen = ChooseParts(catalog, look);
    const Result<std::uint16_t> joint =
        chosen.and_then([&](const ChosenParts&) { return FaceJoint(body); });
    Result<GeometryAsset> head = joint.and_then(
        [&](std::uint16_t) { return BuildPartGeometry(heads, *chosen->headPart, body.scale); });
    if (!head) {
        return head;
    }
    GeometryAsset driver = MergeHead(body, *head, *joint);
    const std::int32_t expression = detail == DriverDetail::Race ? 0 : look.expression;
    std::ranges::replace(driver.materials, kFaceMaterial,
                         FaceMaterial(chosen->face->name, expression));
    std::ranges::replace(driver.materials, kTorsoMaterial, chosen->torso->name);
    std::ranges::replace(driver.materials, kLegsMaterial, chosen->legs->name);
    return driver;
}

}
