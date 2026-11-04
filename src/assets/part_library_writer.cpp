#include "assets/part_library_writer.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>

#include "assets/token_writer.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kPoolField = 0x28;
constexpr FieldId kPartField = 0x2C;
constexpr FieldId kScaleField = 0x2D;
constexpr FieldId kGroupField = 0x2B;
constexpr FieldId kMaterialField = 0x27;
constexpr FieldId kTrianglesField = 0x2A;
constexpr float kUvSteps = 4096.0F;
constexpr float kNormalSteps = 127.0F;

Result<std::int32_t> Int16(float value) {
    const float rounded = std::round(value);
    if (!std::isfinite(rounded) || rounded < std::numeric_limits<std::int16_t>::min() ||
        rounded > std::numeric_limits<std::int16_t>::max()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} doesn't fit a part library's 16-bit values", value));
    }
    return static_cast<std::int32_t>(rounded);
}

Result<void> WritePool(TokenWriter& writer, const std::vector<GeometryVertex>& pool) {
    writer.Field(kPoolField);
    writer.OpenList(pool.size());
    for (const GeometryVertex& vertex : pool) {
        const std::array values{vertex.position.x,
                                vertex.position.y,
                                vertex.position.z,
                                vertex.u * kUvSteps,
                                vertex.v * kUvSteps,
                                vertex.normal.x * kNormalSteps,
                                vertex.normal.y * kNormalSteps,
                                vertex.normal.z * kNormalSteps};
        for (const float value : values) {
            Result<std::int32_t> stored = Int16(value);
            if (!stored) {
                return std::unexpected(std::move(stored.error()));
            }
            writer.Int(*stored);
        }
    }
    writer.Close();
    return {};
}

Result<void> WriteGroup(TokenWriter& writer, const PartGroup& group) {
    writer.Field(kGroupField);
    writer.OpenBlock();
    writer.Field(kMaterialField);
    return writer.String(group.material).transform([&] {
        writer.Field(kTrianglesField);
        writer.OpenList(group.triangles.size());
        for (const std::array<std::int32_t, 3>& triangle : group.triangles) {
            for (const std::int32_t index : triangle) {
                writer.Int(index);
            }
        }
        writer.Close();
        writer.Close();
    });
}

Result<void> WritePart(TokenWriter& writer, const LibraryPart& part) {
    writer.Field(kPartField);
    Result<void> written = writer.String(part.name);
    if (!written) {
        return written;
    }
    writer.OpenBlock();
    writer.Field(kScaleField);
    writer.Float(part.scale);
    writer.Field(kGroupField);
    writer.OpenList(part.groups.size());
    for (const PartGroup& group : part.groups) {
        written = WriteGroup(writer, group);
        if (!written) {
            return written;
        }
    }
    writer.Close();
    writer.Close();
    return {};
}

}

Result<std::vector<std::byte>> WritePartLibrary(const PartLibrary& library) {
    TokenWriter writer;
    Result<void> written = WritePool(writer, library.vertices);
    if (!written) {
        return std::unexpected(std::move(written.error()));
    }
    writer.Field(kPartField);
    writer.OpenList(library.parts.size());
    for (const LibraryPart& part : library.parts) {
        written = WritePart(writer, part);
        if (!written) {
            return std::unexpected(std::move(written.error()));
        }
    }
    writer.Close();
    return std::move(writer).Take();
}

}
