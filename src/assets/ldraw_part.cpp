#include "assets/ldraw_part.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <ranges>
#include <span>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/utf8_path.hpp"
#include "platform/mapped_file.hpp"

namespace rr::assets {
namespace {

constexpr std::array<std::string_view, 3> kSearchFolders{"parts/", "parts/s/", "p/"};
constexpr float kCreaseCosine = 0.866F;
constexpr float kWeldStep = 1000.0F;

struct LdrawTriangle {
    std::int32_t colour = kLdrawMainColour;
    std::array<Vector3, 3> corners{};
};

struct WeldKey {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;

    friend bool operator==(const WeldKey&, const WeldKey&) = default;
};

struct WeldKeyHash {
    std::size_t operator()(const WeldKey& key) const {
        constexpr std::uint64_t kMixY = 0x9E3779B97F4A7C15ULL;
        constexpr std::uint64_t kMixZ = 0xC2B2AE3D27D4EB4FULL;
        return static_cast<std::size_t>(static_cast<std::uint64_t>(key.x) ^
                                        (static_cast<std::uint64_t>(key.y) * kMixY) ^
                                        (static_cast<std::uint64_t>(key.z) * kMixZ));
    }
};

std::string LibraryKey(std::string_view name) {
    std::string key(name);
    std::ranges::replace(key, '\\', '/');
    std::ranges::transform(key, key.begin(), [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
    return key;
}

Vector3 ToGameAxes(const Vector3& ldraw) {
    return Vector3{.x = ldraw.x, .y = ldraw.z, .z = -ldraw.y} * kGameUnitsPerLdu;
}

WeldKey WeldKeyOf(const Vector3& point) {
    return {.x = std::llround(point.x * kWeldStep),
            .y = std::llround(point.y * kWeldStep),
            .z = std::llround(point.z * kWeldStep)};
}

struct Placement {
    LdrawMatrix matrix;
    std::int32_t colour = kLdrawMainColour;
    bool isInverted = false;
    int depth = 0;
};

struct PlacedFile {
    const LdrawFile* file = nullptr;
    Placement placement;
};

class Flattener {
public:
    Flattener(LdrawLibrary& library, const LdrawLimits& limits)
        : library_(&library), limits_(limits) {}

    [[nodiscard]] Result<std::vector<LdrawTriangle>> Flatten(const LdrawFile& file) {
        std::vector<PlacedFile> pending{{.file = &file}};
        while (!pending.empty()) {
            const PlacedFile next = pending.back();
            pending.pop_back();
            Result<void> added = AddFaces(*next.file, next.placement);
            if (!added) {
                return std::unexpected(std::move(added.error()));
            }
            for (const LdrawReference& reference : std::views::reverse(next.file->references)) {
                Result<PlacedFile> sub = Place(reference, next.placement);
                if (!sub) {
                    return std::unexpected(std::move(sub.error()));
                }
                pending.push_back(*sub);
            }
        }
        return std::move(triangles_);
    }

private:
    [[nodiscard]] Result<void> AddFaces(const LdrawFile& file, const Placement& placement) {
        const bool flips = placement.isInverted != (Determinant(placement.matrix) < 0.0F);
        for (const LdrawFace& face : file.faces) {
            const std::int32_t colour =
                face.colour == kLdrawMainColour ? placement.colour : face.colour;
            for (std::uint8_t corner = 2; corner < face.cornerCount; ++corner) {
                if (triangles_.size() >= limits_.maxTriangles) {
                    return Fail(
                        ErrorCode::InvalidData,
                        std::format("LDraw part has over {} triangles", limits_.maxTriangles));
                }
                LdrawTriangle triangle{.colour = colour};
                triangle.corners = {Apply(placement.matrix, face.corners[0]),
                                    Apply(placement.matrix, face.corners.at(corner - 1)),
                                    Apply(placement.matrix, face.corners.at(corner))};
                if (flips) {
                    std::swap(triangle.corners[1], triangle.corners[2]);
                }
                triangles_.push_back(triangle);
            }
        }
        return {};
    }

    [[nodiscard]] Result<PlacedFile> Place(const LdrawReference& reference,
                                           const Placement& placement) {
        if (placement.depth >= limits_.maxDepth) {
            return Fail(ErrorCode::InvalidData,
                        std::format("LDraw sub-files nest deeper than {} at {}", limits_.maxDepth,
                                    reference.file));
        }
        Result<const LdrawFile*> sub = library_->Find(reference.file);
        if (!sub) {
            return std::unexpected(std::move(sub.error()));
        }
        return PlacedFile{
            .file = *sub,
            .placement = {.matrix = Compose(placement.matrix, reference.matrix),
                          .colour = reference.colour == kLdrawMainColour ? placement.colour
                                                                         : reference.colour,
                          .isInverted = placement.isInverted != reference.isInverted,
                          .depth = placement.depth + 1}};
    }

    LdrawLibrary* library_;
    LdrawLimits limits_;
    std::vector<LdrawTriangle> triangles_;
};

std::vector<Vector3> SmoothNormals(const std::vector<LdrawTriangle>& triangles) {
    std::vector<Vector3> faceNormals;
    faceNormals.reserve(triangles.size());
    std::unordered_map<WeldKey, std::vector<std::size_t>, WeldKeyHash> cornersAt;
    for (std::size_t t = 0; t < triangles.size(); ++t) {
        const std::array<Vector3, 3>& c = triangles[t].corners;
        faceNormals.push_back(Cross(c[1] - c[0], c[2] - c[0]));
        for (const Vector3& corner : c) {
            cornersAt[WeldKeyOf(corner)].push_back(t);
        }
    }
    std::vector<Vector3> normals;
    normals.reserve(triangles.size() * 3);
    for (std::size_t t = 0; t < triangles.size(); ++t) {
        const Vector3 own = Normalized(faceNormals[t], {.z = 1.0F});
        for (const Vector3& corner : triangles[t].corners) {
            Vector3 sum{};
            for (const std::size_t other : cornersAt[WeldKeyOf(corner)]) {
                if (Dot(Normalized(faceNormals[other], {}), own) >= kCreaseCosine) {
                    sum += faceNormals[other];
                }
            }
            normals.push_back(Normalized(sum, own));
        }
    }
    return normals;
}

bool HasArea(const LdrawTriangle& triangle) {
    const std::array<Vector3, 3>& c = triangle.corners;
    return Length(Cross(c[1] - c[0], c[2] - c[0])) > 0.0F;
}

GeometryAsset ToGeometry(std::vector<LdrawTriangle> triangles) {
    std::erase_if(triangles, [](const LdrawTriangle& triangle) { return !HasArea(triangle); });
    for (LdrawTriangle& triangle : triangles) {
        for (Vector3& corner : triangle.corners) {
            corner = ToGameAxes(corner);
        }
    }
    std::ranges::stable_sort(triangles, {}, &LdrawTriangle::colour);
    const std::vector<Vector3> normals = SmoothNormals(triangles);
    GeometryAsset geometry;
    geometry.vertices.reserve(normals.size());
    geometry.indices.reserve(normals.size());
    for (std::size_t t = 0; t < triangles.size(); ++t) {
        const std::uint32_t material =
            FindOrAddMaterial(geometry.materials, std::to_string(triangles[t].colour));
        const auto first = static_cast<std::uint32_t>(geometry.indices.size());
        if (geometry.ranges.empty() || geometry.ranges.back().material != material) {
            geometry.ranges.push_back({.firstIndex = first, .material = material});
        }
        for (std::size_t corner = 0; corner < 3; ++corner) {
            geometry.indices.push_back(static_cast<std::uint32_t>(geometry.vertices.size()));
            geometry.vertices.push_back(
                {.position = triangles[t].corners.at(corner), .normal = normals[(t * 3) + corner]});
        }
        geometry.ranges.back().indexCount += 3;
    }
    return geometry;
}

}

Result<const LdrawFile*> LdrawLibrary::Find(std::string_view name) {
    const std::string key = LibraryKey(name);
    if (const auto cached = cache_.find(key); cached != cache_.end()) {
        return &cached->second;
    }
    for (const std::string_view folder : kSearchFolders) {
        const std::filesystem::path path = folder_ / PathFromUtf8(std::string(folder) + key);
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error)) {
            continue;
        }
        Result<LdrawFile> parsed =
            platform::MappedFile::Open(path).and_then([](const platform::MappedFile& mapped) {
                const std::span<const std::byte> bytes = mapped.Bytes();
                return ParseLdraw({reinterpret_cast<const char*>(bytes.data()), bytes.size()});
            });
        if (!parsed) {
            return Fail(parsed.error().code, std::format("{}: {}", key, parsed.error().message));
        }
        return &cache_.emplace(key, std::move(*parsed)).first->second;
    }
    return Fail(ErrorCode::NotFound,
                std::format("LDraw file {} isn't in {}", key, Utf8FromPath(folder_)));
}

Result<GeometryAsset> LdrawLibrary::LoadPart(std::string_view name, const LdrawLimits& limits) {
    return Find(name)
        .and_then([&](const LdrawFile* file) { return Flattener(*this, limits).Flatten(*file); })
        .transform(
            [](std::vector<LdrawTriangle> triangles) { return ToGeometry(std::move(triangles)); });
}

}
