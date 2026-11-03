#include "assets/geometry_writer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "assets/token_writer.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kMaterialsField = 0x27;
constexpr FieldId kLitVerticesField = 0x29;
constexpr FieldId kColoredVerticesField = 0x2A;
constexpr FieldId kTrianglesField = 0x2D;
constexpr FieldId kDrawListField = 0x2E;
constexpr FieldId kScaleField = 0x33;
constexpr FieldId kLoadVertices = 0x31;
constexpr FieldId kDrawTriangles = 0x2D;
constexpr FieldId kUseMaterial = 0x27;
constexpr FieldId kSetMatrix = 0x32;
constexpr std::size_t kCacheSlots = 64;
constexpr std::size_t kMaxBatchTriangles = 255;
constexpr std::size_t kCorners = 3;

using Triangle = std::array<std::int32_t, kCorners>;

struct Command {
    FieldId id = kDrawTriangles;
    std::array<std::int32_t, 3> args{};
    std::size_t argCount = 0;
};

class DrawListBuilder {
public:
    explicit DrawListBuilder(const GeometryAsset& geometry) : geometry_(geometry) {}

    void Add(const GeometryRange& range) {
        const std::span<const std::uint32_t> indices =
            std::span(geometry_.indices).subspan(range.firstIndex, range.indexCount);
        for (std::size_t first = 0; first + kCorners <= indices.size(); first += kCorners) {
            AddTriangle(range.material, indices.subspan(first, kCorners));
        }
    }

    void Flush() {
        if (pending_.empty()) {
            return;
        }
        if (material_ != shownMaterial_) {
            commands_.push_back({.id = kUseMaterial, .args = {Int(*material_)}, .argCount = 1});
            shownMaterial_ = material_;
        }
        LoadBatch();
        commands_.push_back({.id = kDrawTriangles,
                             .args = {Int(triangles_.size()), Int(pending_.size())},
                             .argCount = 2});
        std::ranges::move(pending_, std::back_inserter(triangles_));
        pending_.clear();
        batch_.clear();
    }

    [[nodiscard]] const std::vector<GeometryVertex>& Vertices() const { return vertices_; }
    [[nodiscard]] const std::vector<Triangle>& Triangles() const { return triangles_; }
    [[nodiscard]] const std::vector<Command>& Commands() const { return commands_; }

private:
    static std::int32_t Int(std::size_t value) { return static_cast<std::int32_t>(value); }

    void AddTriangle(std::uint32_t material, std::span<const std::uint32_t> corners) {
        std::size_t added = 0;
        for (std::size_t i = 0; i < corners.size(); ++i) {
            const bool isRepeat = std::ranges::contains(corners.first(i), corners[i]);
            added += !isRepeat && !std::ranges::contains(batch_, corners[i]) ? 1 : 0;
        }
        if (material != material_ || batch_.size() + added > kCacheSlots ||
            pending_.size() == kMaxBatchTriangles) {
            Flush();
            material_ = material;
        }
        Triangle& triangle = pending_.emplace_back();
        for (std::size_t i = 0; i < corners.size(); ++i) {
            triangle.at(i) = SlotOf(corners[i]);
        }
    }

    std::int32_t SlotOf(std::uint32_t vertex) {
        const auto found = std::ranges::find(batch_, vertex);
        if (found != batch_.end()) {
            return Int(static_cast<std::size_t>(found - batch_.begin()));
        }
        batch_.push_back(vertex);
        return Int(batch_.size() - 1);
    }

    void LoadBatch() {
        const std::size_t base = vertices_.size();
        for (std::size_t slot = 0; slot < batch_.size();) {
            const std::uint16_t joint = geometry_.vertices[batch_[slot]].joint;
            std::size_t end = slot;
            while (end < batch_.size() && geometry_.vertices[batch_[end]].joint == joint) {
                vertices_.push_back(geometry_.vertices[batch_[end]]);
                ++end;
            }
            if (joint != matrix_) {
                commands_.push_back({.id = kSetMatrix, .args = {joint}, .argCount = 1});
                matrix_ = joint;
            }
            commands_.push_back({.id = kLoadVertices,
                                 .args = {Int(slot), Int(base + slot), Int(end - slot)},
                                 .argCount = 3});
            slot = end;
        }
    }

    const GeometryAsset& geometry_;
    std::vector<GeometryVertex> vertices_;
    std::vector<Triangle> triangles_;
    std::vector<Command> commands_;
    std::vector<std::uint32_t> batch_;
    std::vector<Triangle> pending_;
    std::optional<std::uint32_t> material_;
    std::optional<std::uint32_t> shownMaterial_;
    std::uint16_t matrix_ = 0;
};

Result<void> WriteMaterials(TokenWriter& writer, const std::vector<std::string>& materials) {
    writer.Field(kMaterialsField);
    writer.OpenList(materials.size());
    for (const std::string& material : materials) {
        if (Result<void> written = writer.String(material); !written) {
            return written;
        }
    }
    writer.Close();
    return {};
}

void WriteVertices(TokenWriter& writer, std::span<const GeometryVertex> vertices,
                   GdbVertices layout, float scale) {
    writer.Field(layout == GdbVertices::Lit ? kLitVerticesField : kColoredVerticesField);
    writer.OpenList(vertices.size());
    for (const GeometryVertex& vertex : vertices) {
        for (const float value : {vertex.position.x / scale, vertex.position.y / scale,
                                  vertex.position.z / scale, vertex.u, vertex.v}) {
            writer.Float(value);
        }
        if (layout == GdbVertices::Lit) {
            for (const float value : {vertex.normal.x, vertex.normal.y, vertex.normal.z}) {
                writer.Float(value);
            }
        } else {
            for (const std::uint8_t channel : vertex.rgba) {
                writer.Int(channel);
            }
        }
    }
    writer.Close();
}

void WriteTriangles(TokenWriter& writer, std::span<const Triangle> triangles) {
    writer.Field(kTrianglesField);
    writer.OpenList(triangles.size());
    for (const Triangle& triangle : triangles) {
        for (const std::int32_t slot : triangle) {
            writer.Int(slot);
        }
    }
    writer.Close();
}

void WriteDrawList(TokenWriter& writer, std::span<const Command> commands) {
    writer.Field(kDrawListField);
    writer.OpenList(commands.size());
    for (const Command& command : commands) {
        writer.Field(command.id);
        for (const std::int32_t arg : std::span(command.args).first(command.argCount)) {
            writer.Int(arg);
        }
    }
    writer.Close();
}

}

Result<std::vector<std::byte>> WriteGeometry(const GeometryAsset& geometry, GdbVertices vertices) {
    if (!geometry.ranges.empty() && geometry.materials.empty()) {
        return Fail(ErrorCode::InvalidData, "a model with triangles needs a material");
    }
    if (!std::isfinite(geometry.scale) || geometry.scale == 0.0F) {
        return Fail(ErrorCode::InvalidData, "a model's scale must be finite and nonzero");
    }
    DrawListBuilder builder(geometry);
    for (const GeometryRange& range : geometry.ranges) {
        builder.Add(range);
    }
    builder.Flush();
    TokenWriter writer;
    return WriteMaterials(writer, geometry.materials).transform([&] {
        WriteVertices(writer, builder.Vertices(), vertices, geometry.scale);
        WriteTriangles(writer, builder.Triangles());
        WriteDrawList(writer, builder.Commands());
        if (geometry.scale != 1.0F) {
            writer.Field(kScaleField);
            writer.Float(geometry.scale);
        }
        return std::move(writer).Take();
    });
}

}
