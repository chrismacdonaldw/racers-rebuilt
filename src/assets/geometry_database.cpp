#include "assets/geometry_database.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr std::size_t kCacheSlots = 64;
constexpr std::size_t kMaxBatchTriangles = 255;
constexpr std::size_t kMaxMatrixDepth = 10;

enum class VertexLayout : std::uint8_t {
    Position,
    Textured,
    Lit,
    Colored,
};

enum class Op : std::uint8_t {
    LoadVertices,
    DrawTriangles,
    UseMaterial,
    SetMatrix,
    PushMatrix,
    PopMatrix,
};

struct Command {
    Op op = Op::PopMatrix;
    std::array<std::int32_t, 3> args{};
};

using Triangle = std::array<std::int32_t, 3>;

struct GdbFile {
    std::vector<std::string> materials;
    std::optional<std::vector<GeometryVertex>> vertices;
    std::vector<Triangle> triangles;
    std::vector<Command> commands;
    float scale = 1.0F;
};

Result<void> ReadFloats(TokenReader& reader, std::initializer_list<float*> targets) {
    for (float* target : targets) {
        Result<void> read = ReadValue(reader, *target);
        if (!read) {
            return read;
        }
    }
    return {};
}

template <VertexLayout Layout>
Result<void> ReadVertex(TokenReader& reader, GeometryVertex& vertex) {
    Vector3& position = vertex.position;
    Result<void> read = ReadFloats(reader, {&position.x, &position.y, &position.z});
    if constexpr (Layout != VertexLayout::Position) {
        read = read.and_then([&] { return ReadFloats(reader, {&vertex.u, &vertex.v}); });
    }
    if constexpr (Layout == VertexLayout::Lit) {
        Vector3& normal = vertex.normal;
        read = read.and_then([&] { return ReadFloats(reader, {&normal.x, &normal.y, &normal.z}); });
    }
    if constexpr (Layout == VertexLayout::Colored) {
        read = read.and_then([&] { return ReadValue(reader, vertex.rgba); });
    }
    return read;
}

template <VertexLayout Layout>
Result<void> ReadVertices(TokenReader& reader, GdbFile& file) {
    if (file.vertices) {
        return Fail(ErrorCode::InvalidData, "a model has more than one vertex list");
    }
    std::vector<GeometryVertex>& vertices = file.vertices.emplace();
    return ReadList(reader, [&] { return ReadVertex<Layout>(reader, vertices.emplace_back()); });
}

template <Op Kind, std::size_t ArgCount>
Result<void> ReadCommand(TokenReader& reader, std::vector<Command>& commands) {
    Command& command = commands.emplace_back(Command{.op = Kind});
    for (std::int32_t& arg : std::span(command.args).first(ArgCount)) {
        Result<void> read = ReadValue(reader, arg);
        if (!read) {
            return read;
        }
    }
    return {};
}

constexpr std::array kCommandRules{
    FieldRule<std::vector<Command>>{.id = 0x31, .read = ReadCommand<Op::LoadVertices, 3>},
    FieldRule<std::vector<Command>>{.id = 0x2D, .read = ReadCommand<Op::DrawTriangles, 2>},
    FieldRule<std::vector<Command>>{.id = 0x27, .read = ReadCommand<Op::UseMaterial, 1>},
    FieldRule<std::vector<Command>>{.id = 0x32, .read = ReadCommand<Op::SetMatrix, 1>},
    FieldRule<std::vector<Command>>{.id = 0x2F, .read = ReadCommand<Op::PushMatrix, 1>},
    FieldRule<std::vector<Command>>{.id = 0x30, .read = ReadCommand<Op::PopMatrix, 0>},
};

Result<void> ReadDrawList(TokenReader& reader, GdbFile& file) {
    file.commands.clear();
    return ReadList(reader, [&] {
        return ReadField<std::vector<Command>>(reader, kCommandRules, file.commands);
    });
}

constexpr std::array kGeometryFields{
    Field<&GdbFile::materials>(0x27),
    FieldRule<GdbFile>{.id = 0x28, .read = ReadVertices<VertexLayout::Textured>},
    FieldRule<GdbFile>{.id = 0x29, .read = ReadVertices<VertexLayout::Lit>},
    FieldRule<GdbFile>{.id = 0x2A, .read = ReadVertices<VertexLayout::Colored>},
    FieldRule<GdbFile>{.id = 0x34, .read = ReadVertices<VertexLayout::Position>},
    Field<&GdbFile::triangles>(0x2D),
    FieldRule<GdbFile>{.id = 0x2E, .read = ReadDrawList},
    Field<&GdbFile::scale>(0x33),
};

bool IsSpan(std::int32_t first, std::int32_t count, std::size_t size) {
    return first >= 0 && count >= 0 &&
           static_cast<std::size_t>(first) + static_cast<std::size_t>(count) <= size;
}

class DrawListReplay {
public:
    explicit DrawListReplay(GdbFile file)
        : vertices_(std::move(file.vertices).value_or(std::vector<GeometryVertex>{})),
          triangles_(std::move(file.triangles)),
          isLoaded_(vertices_.size(), false) {
        geometry_.materials = std::move(file.materials);
    }

    Result<void> Run(const Command& command) {
        const auto [a, b, c] = command.args;
        switch (command.op) {
            case Op::LoadVertices:
                return Load(a, b, c);
            case Op::DrawTriangles:
                return Draw(a, b);
            case Op::UseMaterial:
                return UseMaterial(a);
            case Op::SetMatrix:
                return SetMatrix(a);
            case Op::PushMatrix:
                return PushMatrix(a);
            case Op::PopMatrix:
                return PopMatrix();
        }
        std::unreachable();
    }

    GeometryAsset Finish(float scale) && {
        for (GeometryVertex& vertex : vertices_) {
            vertex.position = {.x = vertex.position.x * scale,
                               .y = vertex.position.y * scale,
                               .z = vertex.position.z * scale};
        }
        geometry_.vertices = std::move(vertices_);
        return std::move(geometry_);
    }

private:
    Result<void> Load(std::int32_t slot, std::int32_t first, std::int32_t count) {
        if (!IsSpan(slot, count, kCacheSlots) || !IsSpan(first, count, vertices_.size())) {
            return Fail(ErrorCode::InvalidData,
                        std::format("loading {} vertices from {} into slot {} is out of range",
                                    count, first, slot));
        }
        const auto firstSlot = static_cast<std::size_t>(slot);
        const auto firstVertex = static_cast<std::uint32_t>(first);
        for (std::uint32_t i = 0; std::cmp_less(i, count); ++i) {
            const std::uint32_t vertex = firstVertex + i;
            if (isLoaded_[vertex] && vertices_[vertex].joint != matrix_) {
                return Fail(ErrorCode::InvalidData,
                            std::format("vertex {} is loaded under two matrices", vertex));
            }
            isLoaded_[vertex] = true;
            vertices_[vertex].joint = matrix_;
            cache_.at(firstSlot + i) = vertex;
        }
        return {};
    }

    Result<void> Draw(std::int32_t first, std::int32_t count) {
        if (!IsSpan(first, count, triangles_.size()) ||
            std::cmp_greater(count, kMaxBatchTriangles)) {
            return Fail(ErrorCode::InvalidData,
                        std::format("drawing {} triangles from {} is out of range", count, first));
        }
        if (geometry_.materials.empty()) {
            return Fail(ErrorCode::InvalidData, "a model draws without any materials");
        }
        OpenRange();
        const std::span<const Triangle> batch =
            std::span(triangles_)
                .subspan(static_cast<std::size_t>(first), static_cast<std::size_t>(count));
        for (const Triangle& triangle : batch) {
            for (const std::int32_t corner : triangle) {
                Result<void> drawn = DrawCorner(corner);
                if (!drawn) {
                    return drawn;
                }
            }
        }
        return {};
    }

    Result<void> DrawCorner(std::int32_t corner) {
        const std::optional<std::uint32_t> vertex =
            IsSpan(corner, 1, kCacheSlots) ? cache_.at(static_cast<std::size_t>(corner))
                                           : std::nullopt;
        if (!vertex) {
            return Fail(ErrorCode::InvalidData,
                        std::format("a triangle corner uses empty cache slot {}", corner));
        }
        geometry_.indices.push_back(*vertex);
        geometry_.ranges.back().indexCount += 1;
        return {};
    }

    void OpenRange() {
        if (geometry_.ranges.empty() || geometry_.ranges.back().material != material_) {
            geometry_.ranges.push_back(
                {.firstIndex = static_cast<std::uint32_t>(geometry_.indices.size()),
                 .material = material_});
        }
    }

    Result<void> UseMaterial(std::int32_t index) {
        if (!IsSpan(index, 1, geometry_.materials.size())) {
            return Fail(ErrorCode::InvalidData, std::format("material {} is past the {} names",
                                                            index, geometry_.materials.size()));
        }
        material_ = static_cast<std::uint32_t>(index);
        return {};
    }

    Result<void> SetMatrix(std::int32_t index) {
        if (!std::in_range<std::uint16_t>(index)) {
            return Fail(ErrorCode::InvalidData, std::format("matrix {} is out of range", index));
        }
        matrix_ = static_cast<std::uint16_t>(index);
        return {};
    }

    Result<void> PushMatrix(std::int32_t index) {
        if (matrixStack_.size() == kMaxMatrixDepth) {
            return Fail(ErrorCode::InvalidData, "the matrix stack is full");
        }
        return SetMatrix(index).transform([&] { matrixStack_.push_back(matrix_); });
    }

    Result<void> PopMatrix() {
        if (matrixStack_.empty()) {
            return Fail(ErrorCode::InvalidData, "the matrix stack is empty");
        }
        matrixStack_.pop_back();
        matrix_ = matrixStack_.empty() ? std::uint16_t{0} : matrixStack_.back();
        return {};
    }

    std::vector<GeometryVertex> vertices_;
    std::vector<Triangle> triangles_;
    std::vector<bool> isLoaded_;
    std::array<std::optional<std::uint32_t>, kCacheSlots> cache_{};
    std::uint32_t material_ = 0;
    std::uint16_t matrix_ = 0;
    std::vector<std::uint16_t> matrixStack_;
    GeometryAsset geometry_;
};

}

Result<GeometryAsset> ParseGeometry(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    GdbFile file;
    Result<void> read = ReadFields<GdbFile>(reader, kGeometryFields, file).and_then([&] {
        return reader.Expect(TokenKind::End).transform([](const Token&) {});
    });
    if (!read) {
        return std::unexpected(std::move(read.error()));
    }
    const float scale = file.scale;
    const std::vector<Command> commands = std::move(file.commands);
    DrawListReplay replay(std::move(file));
    for (const Command& command : commands) {
        Result<void> ran = replay.Run(command);
        if (!ran) {
            return std::unexpected(std::move(ran.error()));
        }
    }
    return std::move(replay).Finish(scale);
}

}
