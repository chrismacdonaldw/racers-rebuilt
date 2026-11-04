#include "assets/part_library.hpp"

#include <algorithm>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/merged_table.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kPartField = 0x2C;
constexpr FieldId kGroupField = 0x2B;
constexpr float kUvScale = 1.0F / 4096.0F;
constexpr float kNormalScale = 1.0F / 127.0F;
constexpr std::uint32_t kUnmapped = std::numeric_limits<std::uint32_t>::max();

float AsInt16(std::int32_t value) {
    return static_cast<float>(static_cast<std::int16_t>(value));
}

float AsInt8(std::int32_t value) {
    return static_cast<float>(static_cast<std::int8_t>(value));
}

Result<void> ReadPoolVertex(TokenReader& reader, GeometryVertex& vertex) {
    std::array<std::int32_t, 8> values{};
    return ReadValue(reader, values).transform([&] {
        const auto [x, y, z, u, v, nx, ny, nz] = values;
        vertex.position = {.x = AsInt16(x), .y = AsInt16(y), .z = AsInt16(z)};
        vertex.u = AsInt16(u) * kUvScale;
        vertex.v = AsInt16(v) * kUvScale;
        vertex.normal = {.x = AsInt8(nx) * kNormalScale,
                         .y = AsInt8(ny) * kNormalScale,
                         .z = AsInt8(nz) * kNormalScale};
    });
}

Result<void> ReadPool(TokenReader& reader, PartLibrary& library) {
    library.vertices.clear();
    return ReadList(reader,
                    [&] { return ReadPoolVertex(reader, library.vertices.emplace_back()); });
}

constexpr std::array kGroupFields{
    Field<&PartGroup::material>(0x27),
    Field<&PartGroup::triangles>(0x2A),
};

Result<void> ReadGroups(TokenReader& reader, LibraryPart& part) {
    part.groups.clear();
    return ReadList(reader, [&] {
        return ExpectField(reader, kGroupField).and_then([&] {
            return ReadBlock<PartGroup>(reader, kGroupFields, part.groups.emplace_back(),
                                        FieldRepeats::Fail);
        });
    });
}

constexpr std::array kPartFields{
    Field<&LibraryPart::scale>(0x2D),
    FieldRule<LibraryPart>{.id = kGroupField, .read = ReadGroups},
};

Result<void> ReadPart(TokenReader& reader, LibraryPart& part) {
    return ExpectField(reader, kPartField)
        .and_then([&] { return reader.ReadString(); })
        .and_then([&](std::string_view name) {
            part.name = name;
            return ReadBlock<LibraryPart>(reader, kPartFields, part, FieldRepeats::Fail);
        });
}

Result<void> ReadParts(TokenReader& reader, PartLibrary& library) {
    library.parts.clear();
    return ReadList(reader, [&] { return ReadPart(reader, library.parts.emplace_back()); });
}

constexpr std::array kLibraryFields{
    FieldRule<PartLibrary>{.id = 0x28, .read = ReadPool},
    FieldRule<PartLibrary>{.id = kPartField, .read = ReadParts},
};

Result<void> CheckIndices(const LibraryPart& part, std::size_t poolSize) {
    for (const PartGroup& group : part.groups) {
        for (const std::array<std::int32_t, 3>& triangle : group.triangles) {
            const auto outside = std::ranges::find_if(triangle, [&](std::int32_t index) {
                return index < 0 || std::cmp_greater_equal(index, poolSize);
            });
            if (outside != triangle.end()) {
                return Fail(ErrorCode::InvalidData, std::format("part {} uses vertex {} of {}",
                                                                part.name, *outside, poolSize));
            }
        }
    }
    return {};
}

Result<void> CheckIndices(const PartLibrary& library) {
    for (const LibraryPart& part : library.parts) {
        Result<void> checked = CheckIndices(part, library.vertices.size());
        if (!checked) {
            return checked;
        }
    }
    return {};
}

class PartCopy {
public:
    PartCopy(const std::vector<GeometryVertex>& pool, float scale)
        : pool_(pool), remap_(pool.size(), kUnmapped) {
        geometry_.scale = scale;
    }

    void AddGroup(const PartGroup& group) {
        const auto firstIndex = static_cast<std::uint32_t>(geometry_.indices.size());
        for (const std::array<std::int32_t, 3>& triangle : group.triangles) {
            for (const std::int32_t index : triangle) {
                geometry_.indices.push_back(Remap(static_cast<std::size_t>(index)));
            }
        }
        geometry_.ranges.push_back({
            .firstIndex = firstIndex,
            .indexCount = static_cast<std::uint32_t>(geometry_.indices.size()) - firstIndex,
            .material = FindOrAddMaterial(geometry_.materials, group.material),
        });
    }

    GeometryAsset Finish() && { return std::move(geometry_); }

private:
    std::uint32_t Remap(std::size_t index) {
        std::uint32_t& mapped = remap_[index];
        if (mapped == kUnmapped) {
            mapped = static_cast<std::uint32_t>(geometry_.vertices.size());
            GeometryVertex vertex = pool_[index];
            vertex.position = {.x = vertex.position.x * geometry_.scale,
                               .y = vertex.position.y * geometry_.scale,
                               .z = vertex.position.z * geometry_.scale};
            geometry_.vertices.push_back(vertex);
        }
        return mapped;
    }

    const std::vector<GeometryVertex>& pool_;
    std::vector<std::uint32_t> remap_;
    GeometryAsset geometry_;
};

void ShiftIndices(LibraryPart& part, std::int32_t offset) {
    for (PartGroup& group : part.groups) {
        for (std::array<std::int32_t, 3>& triangle : group.triangles) {
            for (std::int32_t& index : triangle) {
                index += offset;
            }
        }
    }
}

}

Result<PartLibrary> ParsePartLibrary(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    PartLibrary library;
    return ReadFields<PartLibrary>(reader, kLibraryFields, library, FieldRepeats::Fail)
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .and_then([&](const Token&) { return CheckIndices(library); })
        .transform([&] { return std::move(library); });
}

PartLibrary MergePartLibraries(std::vector<PartLibrary> libraries) {
    PartLibrary merged;
    std::vector<std::vector<LibraryPart>> parts;
    parts.reserve(libraries.size());
    for (PartLibrary& library : libraries) {
        const auto offset = static_cast<std::int32_t>(merged.vertices.size());
        for (LibraryPart& part : library.parts) {
            ShiftIndices(part, offset);
        }
        merged.vertices.insert(merged.vertices.end(), library.vertices.begin(),
                               library.vertices.end());
        parts.push_back(std::move(library.parts));
    }
    merged.parts = MergeNamed(std::move(parts));
    return merged;
}

void AddLibraryPart(PartLibrary& library, std::string name, const GeometryAsset& geometry) {
    const auto first = static_cast<std::int32_t>(library.vertices.size());
    library.vertices.insert(library.vertices.end(), geometry.vertices.begin(),
                            geometry.vertices.end());
    LibraryPart part{.name = std::move(name)};
    for (const GeometryRange& range : geometry.ranges) {
        const std::string& material = geometry.materials.at(range.material);
        auto group = std::ranges::find(part.groups, material, &PartGroup::material);
        if (group == part.groups.end()) {
            group = part.groups.insert(part.groups.end(), PartGroup{.material = material});
        }
        const std::span<const std::uint32_t> indices =
            std::span(geometry.indices).subspan(range.firstIndex, range.indexCount);
        for (std::size_t corner = 0; corner + 2 < indices.size(); corner += 3) {
            group->triangles.push_back({first + static_cast<std::int32_t>(indices[corner]),
                                        first + static_cast<std::int32_t>(indices[corner + 1]),
                                        first + static_cast<std::int32_t>(indices[corner + 2])});
        }
    }
    library.parts.push_back(std::move(part));
}

Result<GeometryAsset> BuildPartGeometry(const PartLibrary& library, std::string_view name,
                                        float scale) {
    const std::string upper = ToUpperAscii(name);
    const auto part = std::ranges::find_if(
        library.parts, [&](const LibraryPart& kept) { return ToUpperAscii(kept.name) == upper; });
    if (part == library.parts.end()) {
        return Fail(ErrorCode::NotFound, std::format("the part library has no part {}", name));
    }
    PartCopy copy(library.vertices, scale);
    for (const PartGroup& group : part->groups) {
        copy.AddGroup(group);
    }
    return std::move(copy).Finish();
}

}
