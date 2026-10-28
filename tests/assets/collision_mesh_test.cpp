#include "assets/collision_mesh.hpp"

#include <doctest/doctest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

constexpr std::int32_t kUnitNormal = 1 << 30;

std::vector<std::byte> Int(std::int32_t value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    return test::Bytes(
        {0x04, static_cast<std::uint8_t>(bits), static_cast<std::uint8_t>(bits >> 8U),
         static_cast<std::uint8_t>(bits >> 16U), static_cast<std::uint8_t>(bits >> 24U)});
}

std::vector<std::byte> Ints(std::initializer_list<std::int32_t> values) {
    std::vector<std::byte> bytes;
    for (const std::int32_t value : values) {
        const std::vector<std::byte> encoded = Int(value);
        bytes.insert(bytes.end(), encoded.begin(), encoded.end());
    }
    return bytes;
}

std::vector<std::byte> Block(std::uint8_t field, std::uint8_t count, std::vector<std::byte> body) {
    return test::Join(
        {test::Bytes({field}), test::ListHeader(count), std::move(body), test::Bytes({0x06})});
}

std::vector<std::byte> Materials() {
    return Block(0x27, 1, test::Text("road"));
}

std::vector<std::byte> Vertices() {
    return Block(0x34, 3, test::Floats({0, 0, 0, 10, 0, 0, 0, 10, 0}));
}

std::vector<std::byte> Triangles(std::initializer_list<std::int32_t> triangle) {
    return Block(0x2D, 1, Ints(triangle));
}

std::vector<std::byte> FixedNodes(std::initializer_list<std::int32_t> node) {
    return Block(0x8E, 1, Ints(node));
}

std::vector<std::byte> Floor() {
    return test::Join({Materials(), Vertices(), Triangles({0, 1, 2, 0}),
                       FixedNodes({-1, -2, 0, 0, kUnitNormal, 0, 1})});
}

TEST_CASE("a collision mesh parses fixed-point nodes") {
    const Result<CollisionMeshAsset> mesh = ParseCollisionMesh(Floor());

    REQUIRE(mesh.has_value());
    CHECK(mesh->materials == std::vector<std::string>{"road"});
    CHECK(mesh->vertices.size() == 3);
    CHECK(mesh->vertices[1] == Vector3{.x = 10.0F});
    CHECK(mesh->triangles[0].vertices == std::array<std::uint16_t, 3>{0, 1, 2});
    REQUIRE(mesh->nodes.size() == 1);
    const CollisionNode& node = mesh->nodes[0];
    CHECK(node.front == kNoChild);
    CHECK(node.back == kNoChild);
    CHECK(node.normal == Vector3{.z = 1.0F});
    CHECK(node.firstTriangle == 0);
    CHECK(node.triangleCount == 1);
}

TEST_CASE("float nodes take bare child fields and drop the fourth normal value") {
    const std::vector<std::byte> floatNode =
        test::Join({test::Bytes({0x8C, 0x8D}), test::Floats({0, 0, 1, 7}), Ints({0, 1})});
    const std::vector<std::byte> bytes =
        test::Join({Materials(), Vertices(), Triangles({0, 1, 2, 0}), Block(0x8B, 1, floatNode)});

    const Result<CollisionMeshAsset> mesh = ParseCollisionMesh(bytes);

    REQUIRE(mesh.has_value());
    CHECK(mesh->nodes[0].front == kNoChild);
    CHECK(mesh->nodes[0].back == kNoChild);
    CHECK(mesh->nodes[0].normal == Vector3{.z = 1.0F});
}

bool ParsesWithTriangle(std::initializer_list<std::int32_t> triangle) {
    return ParseCollisionMesh(test::Join({Materials(), Vertices(), Triangles(triangle),
                                          FixedNodes({-1, -2, 0, 0, kUnitNormal, 0, 1})}))
        .has_value();
}

bool ParsesWithNode(std::initializer_list<std::int32_t> node) {
    return ParseCollisionMesh(
               test::Join({Materials(), Vertices(), Triangles({0, 1, 2, 0}), FixedNodes(node)}))
        .has_value();
}

TEST_CASE("a collision mesh rejects triangle indices past their lists") {
    CHECK_FALSE(ParsesWithTriangle({0, 1, 3, 0}));
    CHECK_FALSE(ParsesWithTriangle({0, 1, 2, 1}));
}

TEST_CASE("a collision mesh rejects node children and triangle runs past their lists") {
    CHECK_FALSE(ParsesWithNode({1, -2, 0, 0, kUnitNormal, 0, 1}));
    CHECK_FALSE(ParsesWithNode({-1, -3, 0, 0, kUnitNormal, 0, 1}));
    CHECK_FALSE(ParsesWithNode({-1, -2, 0, 0, kUnitNormal, 0, 2}));
    CHECK_FALSE(ParsesWithNode({-1, -2, 0, 0, kUnitNormal, 0, 0}));
    CHECK_FALSE(ParsesWithNode({-1, -2, 0, 0, kUnitNormal, 1, 0x7FFFFFFF}));
}

TEST_CASE("a collision mesh rejects empty, repeated and missing blocks") {
    const std::vector<std::byte> empty =
        test::Join({Block(0x27, 0, {}), Vertices(), Triangles({0, 1, 2, 0}),
                    FixedNodes({-1, -2, 0, 0, kUnitNormal, 0, 1})});
    const std::vector<std::byte> repeated = test::Join({Floor(), Vertices()});
    const std::vector<std::byte> noNodes =
        test::Join({Materials(), Vertices(), Triangles({0, 1, 2, 0})});

    CHECK_FALSE(ParseCollisionMesh(empty).has_value());
    CHECK_FALSE(ParseCollisionMesh(repeated).has_value());
    CHECK_FALSE(ParseCollisionMesh(noNodes).has_value());
}

TEST_CASE("every collision mesh in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    std::size_t nodeCount = 0;
    const test::Survey meshes = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "BVB"; },
        [&](std::span<const std::byte> bytes) {
            return ParseCollisionMesh(bytes).transform(
                [&](const CollisionMeshAsset& mesh) { nodeCount += mesh.nodes.size(); });
        });
    MESSAGE("BVB: ", meshes.fileCount, " files, ", nodeCount, " nodes");
    test::CheckSurvey(meshes, 45);
    CHECK(nodeCount == 33959);
}

}
}
