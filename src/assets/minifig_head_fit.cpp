#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

#include "assets/minifig_fit.hpp"
#include "assets/surface_transfer.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kFaceMaterial = "face";

GeometryAsset HeadOnNeck(GeometryAsset head) {
    Vector3 low{.x = std::numeric_limits<float>::max(),
                .y = std::numeric_limits<float>::max(),
                .z = std::numeric_limits<float>::max()};
    Vector3 high{.x = std::numeric_limits<float>::lowest(),
                 .y = std::numeric_limits<float>::lowest(),
                 .z = std::numeric_limits<float>::lowest()};
    for (GeometryVertex& vertex : head.vertices) {
        vertex.position = {.x = -vertex.position.y, .y = vertex.position.x, .z = vertex.position.z};
        vertex.normal = {.x = -vertex.normal.y, .y = vertex.normal.x, .z = vertex.normal.z};
        low = {.x = std::min(low.x, vertex.position.x),
               .y = std::min(low.y, vertex.position.y),
               .z = std::min(low.z, vertex.position.z)};
        high = {.x = std::max(high.x, vertex.position.x),
                .y = std::max(high.y, vertex.position.y),
                .z = std::max(high.z, vertex.position.z)};
    }
    const Vector3 shift{.x = -(low.x + high.x) / 2.0F, .y = -(low.y + high.y) / 2.0F, .z = -low.z};
    for (GeometryVertex& vertex : head.vertices) {
        vertex.position += shift;
    }
    return head;
}

std::vector<SurfaceTriangle> FaceSurface(const PartLibrary& heads, const LibraryPart& part,
                                         const PartGroup& face) {
    std::vector<SurfaceTriangle> surface;
    surface.reserve(face.triangles.size());
    for (const std::array<std::int32_t, 3>& indices : face.triangles) {
        std::array<Vector3, 3> corners;
        for (std::size_t corner = 0; corner < 3; ++corner) {
            corners.at(corner) =
                heads.vertices.at(static_cast<std::size_t>(indices.at(corner))).position *
                part.scale;
        }
        SurfaceTriangle& triangle = surface.emplace_back(MakeSurfaceTriangle(corners));
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const GeometryVertex& vertex =
                heads.vertices.at(static_cast<std::size_t>(indices.at(corner)));
            triangle.u.at(corner) = vertex.u;
            triangle.v.at(corner) = vertex.v;
        }
    }
    return surface;
}

void SwapFace(PartLibrary& heads, const LibraryPart& part, PartGroup& face,
              const GeometryAsset& head) {
    const std::vector<SurfaceTriangle> surface = FaceSurface(heads, part, face);
    face.triangles.clear();
    for (std::size_t first = 0; first + 2 < head.indices.size(); first += 3) {
        std::array<const GeometryVertex*, 3> corners{};
        for (std::size_t corner = 0; corner < 3; ++corner) {
            corners.at(corner) = &head.vertices.at(head.indices[first + corner]);
        }
        const Vector3 facing = Normalized(
            corners[0]->normal + corners[1]->normal + corners[2]->normal, corners[0]->normal);
        const std::array<SurfacePoint, 3> sources = NearestSurfacePoints(
            surface, {corners[0]->position, corners[1]->position, corners[2]->position}, facing);
        std::array<std::int32_t, 3> triangle{};
        for (std::size_t corner = 0; corner < 3; ++corner) {
            triangle.at(corner) = static_cast<std::int32_t>(heads.vertices.size());
            heads.vertices.push_back(
                {.position = corners.at(corner)->position * (1.0F / part.scale),
                 .normal = corners.at(corner)->normal,
                 .u = sources.at(corner).u,
                 .v = sources.at(corner).v,
                 .rgba = kOpaqueWhite,
                 .joint = 0});
        }
        face.triangles.push_back(triangle);
    }
}

}

PartLibrary FitMinifigHeads(PartLibrary heads, const GeometryAsset& head) {
    const GeometryAsset placed = HeadOnNeck(head);
    for (LibraryPart& part : heads.parts) {
        const auto face = std::ranges::find(part.groups, kFaceMaterial, &PartGroup::material);
        if (face != part.groups.end() && part.scale != 0.0F) {
            SwapFace(heads, part, *face, placed);
        }
    }
    return heads;
}

}
