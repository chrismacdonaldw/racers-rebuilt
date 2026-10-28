#include "assets/collision_mesh.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <limits>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr std::int32_t kEmptyChild = -1;
constexpr std::int32_t kSolidChild = -2;
constexpr FieldId kEmptyChildField = 0x8C;
constexpr FieldId kSolidChildField = 0x8D;
constexpr float kFixedNormalScale = 1.0F / 1073741824.0F;
constexpr std::size_t kMaxIndexedCount = std::numeric_limits<std::uint16_t>::max();
constexpr std::size_t kMaxNodeCount = 0xFFFE;

using StoredTriangle = std::array<std::int32_t, 4>;
using FixedNode = std::array<std::int32_t, 7>;

struct StoredNode {
    std::int32_t front = kNoChild;
    std::int32_t back = kNoChild;
    Vector3 normal;
    std::int32_t firstTriangle = 0;
    std::int32_t triangleCount = 0;
};

struct StoredMesh {
    std::vector<std::string> materials;
    std::vector<Vector3> vertices;
    std::vector<StoredTriangle> triangles;
    std::vector<FixedNode> fixedNodes;
    std::vector<StoredNode> floatNodes;
};

Result<void> ReadChild(TokenReader& reader, std::int32_t& child) {
    Result<Token> token = reader.Peek();
    if (!token) {
        return std::unexpected(std::move(token.error()));
    }
    if (token->kind != TokenKind::Field) {
        return ReadValue(reader, child);
    }
    return reader.Next().and_then([&child](const Token& field) -> Result<void> {
        if (field.field != kEmptyChildField && field.field != kSolidChildField) {
            return Fail(ErrorCode::InvalidData,
                        std::format("field 0x{:02X} at offset {} is not a child", field.field,
                                    field.offset));
        }
        child = field.field == kEmptyChildField ? kEmptyChild : kSolidChild;
        return {};
    });
}

Result<void> ReadValue(TokenReader& reader, StoredNode& node) {
    float ignored = 0.0F;
    return ReadChild(reader, node.front)
        .and_then([&] { return ReadChild(reader, node.back); })
        .and_then([&] {
            return ReadFloats(reader, {&node.normal.x, &node.normal.y, &node.normal.z, &ignored});
        })
        .and_then([&] { return ReadValue(reader, node.firstTriangle); })
        .and_then([&] { return ReadValue(reader, node.triangleCount); });
}

template <FieldId Id, auto Member>
constexpr FieldRule<StoredMesh> BlockField() {
    return {
        .id = Id,
        .read = [](TokenReader& reader, StoredMesh& mesh) -> Result<void> {
            auto& list = mesh.*Member;
            if (!list.empty()) {
                return Fail(ErrorCode::InvalidData, std::format("a second 0x{:02X} block", Id));
            }
            return ReadList(reader, [&] { return ReadValue(reader, list.emplace_back()); })
                .and_then([&]() -> Result<void> {
                    if (list.empty()) {
                        return Fail(ErrorCode::InvalidData,
                                    std::format("block 0x{:02X} is empty", Id));
                    }
                    return {};
                });
        },
    };
}

constexpr std::array kMeshFields{
    BlockField<0x27, &StoredMesh::materials>(),  BlockField<0x34, &StoredMesh::vertices>(),
    BlockField<0x2D, &StoredMesh::triangles>(),  BlockField<0x8E, &StoredMesh::fixedNodes>(),
    BlockField<0x8B, &StoredMesh::floatNodes>(),
};

bool IsIndex(std::int32_t value, std::size_t count) {
    return value >= 0 && std::cmp_less(value, count);
}

Result<CollisionTriangle> DecodeTriangle(const StoredTriangle& stored, const StoredMesh& mesh) {
    CollisionTriangle triangle;
    for (std::size_t i = 0; i < triangle.vertices.size(); ++i) {
        if (!IsIndex(stored.at(i), mesh.vertices.size())) {
            return Fail(ErrorCode::InvalidData,
                        std::format("triangle vertex {} is past {} vertices", stored.at(i),
                                    mesh.vertices.size()));
        }
        triangle.vertices.at(i) = static_cast<std::uint16_t>(stored.at(i));
    }
    if (!IsIndex(stored[3], mesh.materials.size())) {
        return Fail(ErrorCode::InvalidData, std::format("triangle material {} is past {} materials",
                                                        stored[3], mesh.materials.size()));
    }
    triangle.material = static_cast<std::uint16_t>(stored[3]);
    return triangle;
}

StoredNode FromFixed(const FixedNode& fixed) {
    const auto scaled = [](std::int32_t value) {
        return static_cast<float>(value) * kFixedNormalScale;
    };
    return {.front = fixed[0],
            .back = fixed[1],
            .normal = {.x = scaled(fixed[2]), .y = scaled(fixed[3]), .z = scaled(fixed[4])},
            .firstTriangle = fixed[5],
            .triangleCount = fixed[6]};
}

Result<std::int32_t> DecodeChild(std::int32_t child, std::size_t nodeCount) {
    if (child == kEmptyChild || child == kSolidChild) {
        return kNoChild;
    }
    if (!IsIndex(child, nodeCount)) {
        return Fail(ErrorCode::InvalidData,
                    std::format("node child {} is past {} nodes", child, nodeCount));
    }
    return child;
}

Result<CollisionNode> DecodeNode(const StoredNode& stored, std::size_t nodeCount,
                                 std::size_t triangleCount) {
    const std::int64_t end = std::int64_t{stored.firstTriangle} + stored.triangleCount;
    const bool hasTriangles = stored.firstTriangle >= 0 && stored.triangleCount > 0 &&
                              std::cmp_less_equal(end, triangleCount);
    if (!hasTriangles) {
        return Fail(ErrorCode::InvalidData,
                    std::format("node triangles {} to {} are outside {} triangles",
                                stored.firstTriangle, end, triangleCount));
    }
    CollisionNode node{.normal = stored.normal,
                       .firstTriangle = static_cast<std::uint16_t>(stored.firstTriangle),
                       .triangleCount = static_cast<std::uint16_t>(stored.triangleCount)};
    return DecodeChild(stored.front, nodeCount)
        .and_then([&](std::int32_t front) {
            node.front = front;
            return DecodeChild(stored.back, nodeCount);
        })
        .transform([&](std::int32_t back) {
            node.back = back;
            return node;
        });
}

Result<std::vector<StoredNode>> TakeNodes(StoredMesh& stored) {
    if (stored.fixedNodes.empty() == stored.floatNodes.empty()) {
        return Fail(ErrorCode::InvalidData, "a collision mesh needs exactly one node block");
    }
    if (stored.fixedNodes.empty()) {
        return std::move(stored.floatNodes);
    }
    std::vector<StoredNode> nodes;
    nodes.reserve(stored.fixedNodes.size());
    for (const FixedNode& fixed : stored.fixedNodes) {
        nodes.push_back(FromFixed(fixed));
    }
    return nodes;
}

Result<void> CheckCounts(const StoredMesh& stored) {
    if (stored.materials.empty() || stored.vertices.empty() || stored.triangles.empty()) {
        return Fail(ErrorCode::InvalidData,
                    "a collision mesh needs materials, vertices and triangles");
    }
    if (stored.vertices.size() > kMaxIndexedCount || stored.triangles.size() > kMaxIndexedCount) {
        return Fail(ErrorCode::InvalidData,
                    "a collision mesh has more vertices or triangles than 16-bit indices reach");
    }
    return {};
}

Result<CollisionMeshAsset> Decode(StoredMesh& stored) {
    Result<std::vector<StoredNode>> nodes =
        CheckCounts(stored).and_then([&] { return TakeNodes(stored); });
    if (!nodes) {
        return std::unexpected(std::move(nodes.error()));
    }
    if (nodes->size() > kMaxNodeCount) {
        return Fail(ErrorCode::InvalidData, std::format("{} nodes is too many", nodes->size()));
    }
    CollisionMeshAsset mesh;
    for (const StoredTriangle& triangle : stored.triangles) {
        Result<CollisionTriangle> decoded = DecodeTriangle(triangle, stored);
        if (!decoded) {
            return std::unexpected(std::move(decoded.error()));
        }
        mesh.triangles.push_back(*decoded);
    }
    for (const StoredNode& node : *nodes) {
        Result<CollisionNode> decoded = DecodeNode(node, nodes->size(), mesh.triangles.size());
        if (!decoded) {
            return std::unexpected(std::move(decoded.error()));
        }
        mesh.nodes.push_back(*decoded);
    }
    mesh.materials = std::move(stored.materials);
    mesh.vertices = std::move(stored.vertices);
    return mesh;
}

}

Result<CollisionMeshAsset> ParseCollisionMesh(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    StoredMesh stored;
    return ReadFields<StoredMesh>(reader, kMeshFields, stored)
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .and_then([&](const Token&) { return Decode(stored); });
}

}
