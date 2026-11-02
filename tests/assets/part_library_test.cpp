#include "assets/part_library.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/driver_parts.hpp"
#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Block;
using test::Bytes;
using test::Floats;
using test::Ints;
using test::Join;
using test::ListHeader;
using test::Text;

std::vector<std::byte> List(std::uint8_t count, const std::vector<std::byte>& body) {
    return Join({ListHeader(count), body, Bytes({0x06})});
}

std::vector<std::byte> Group(std::string_view material, std::uint8_t triangles,
                             const std::vector<std::byte>& indices) {
    return Join({Bytes({0x2B}), Block(Join({Bytes({0x27}), Text(material), Bytes({0x2A}),
                                            List(triangles, indices)}))});
}

std::vector<std::byte> Part(std::string_view name, const std::vector<std::byte>& groups,
                            std::uint8_t groupCount) {
    return Join(
        {Bytes({0x2C}), Text(name),
         Block(Join({Bytes({0x2D}), Floats({2}), Bytes({0x2B}), List(groupCount, groups)}))});
}

std::vector<std::byte> FourVertexPool() {
    return Join(
        {Bytes({0x28}),
         List(4, Join({Ints({1, 2, 3, 0, 128, 127, 0, 0}), Ints({4, 5, 6, 64, 0, 0, 129, 0}),
                       Ints({7, 8, 9, 0, 0, 0, 0, 127}), Ints({10, 11, 12, 0, 0, 0, 0, 0})}))});
}

std::vector<std::byte> SmallLibrary() {
    return Join({
        FourVertexPool(),
        Bytes({0x2C}),
        List(
            2,
            Join({Part("head",
                       Join({Group("face", 1, Ints({3, 1, 2})), Group("head", 1, Ints({1, 2, 3}))}),
                       2),
                  Part("cap", Group("cap", 1, Ints({0, 1, 2})), 1)})),
    });
}

TEST_CASE("a part library reads its vertex pool and named parts") {
    const Result<PartLibrary> library = ParsePartLibrary(SmallLibrary());

    REQUIRE(library.has_value());
    REQUIRE(library->vertices.size() == 4);
    const GeometryVertex& first = library->vertices[0];
    CHECK(first.position.x == 1.0F);
    CHECK(first.position.z == 3.0F);
    CHECK(first.v == doctest::Approx(128.0F / 4096.0F));
    CHECK(first.normal.x == doctest::Approx(1.0F));
    CHECK(library->vertices[1].normal.y == doctest::Approx(-1.0F));
    REQUIRE(library->parts.size() == 2);
    const LibraryPart& head = library->parts[0];
    CHECK(head.name == "head");
    CHECK(head.scale == 2.0F);
    REQUIRE(head.groups.size() == 2);
    CHECK(head.groups[0].material == "face");
    CHECK(head.groups[0].triangles == std::vector<std::array<std::int32_t, 3>>{{3, 1, 2}});
}

TEST_CASE("a part copies only the vertices it uses, at the given scale") {
    const Result<PartLibrary> library = ParsePartLibrary(SmallLibrary());
    REQUIRE(library.has_value());

    const Result<GeometryAsset> head = BuildPartGeometry(*library, "head", 0.5F);

    REQUIRE(head.has_value());
    CHECK(head->scale == 0.5F);
    REQUIRE(head->vertices.size() == 3);
    CHECK(head->vertices[0].position.x == 5.0F);
    CHECK(head->vertices[1].position.y == 2.5F);
    CHECK(head->indices == std::vector<std::uint32_t>{0, 1, 2, 1, 2, 0});
    CHECK(head->materials == std::vector<std::string>{"face", "head"});
    CHECK(head->ranges ==
          std::vector<GeometryRange>{{.firstIndex = 0, .indexCount = 3},
                                     {.firstIndex = 3, .indexCount = 3, .material = 1}});
}

TEST_CASE("building a missing part fails") {
    const Result<PartLibrary> library = ParsePartLibrary(SmallLibrary());
    REQUIRE(library.has_value());

    CHECK_FALSE(BuildPartGeometry(*library, "hat", 1.0F).has_value());
}

TEST_CASE("a part that indexes past the vertex pool fails") {
    const std::vector<std::byte> gcb =
        Join({FourVertexPool(), Bytes({0x2C}),
              List(1, Part("cap", Group("cap", 1, Ints({0, 1, 4})), 1))});

    CHECK_FALSE(ParsePartLibrary(gcb).has_value());
}

bool HasUnitNormals(const PartLibrary& library) {
    return std::ranges::all_of(library.vertices, [](const GeometryVertex& vertex) {
        const Vector3& normal = vertex.normal;
        const float length =
            std::sqrt((normal.x * normal.x) + (normal.y * normal.y) + (normal.z * normal.z));
        return std::abs(length - 1.0F) < 0.02F;
    });
}

std::vector<std::string> UnbuildableParts(const PartLibrary& library,
                                          const std::vector<std::string>& names) {
    return names | std::views::filter([&](const std::string& name) {
               const Result<GeometryAsset> part = BuildPartGeometry(library, name, 1.0F);
               return !part.has_value() || part->indices.empty();
           }) |
           std::ranges::to<std::vector>();
}

void CheckRetailLibrary(const Vfs& vfs, std::string_view path, std::size_t partCount,
                        const std::vector<std::string>& headParts) {
    CAPTURE(path);
    const Result<PartLibrary> library = vfs.Read(path).and_then(
        [](const FileData& data) { return ParsePartLibrary(data.Bytes()); });
    REQUIRE(library.has_value());
    CHECK(library->parts.size() == partCount);
    CHECK(HasUnitNormals(*library));
    CHECK(UnbuildableParts(*library, headParts).empty());
}

TEST_CASE("every retail hat has its head part in both part libraries") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<DriverPartCatalog> catalog =
        vfs->Read("MENUDATA/PARTDB/BODYPART.PCB").and_then([](const FileData& data) {
            return ParseDriverParts(data.Bytes());
        });
    REQUIRE(catalog.has_value());

    CheckRetailLibrary(*vfs, "MENUDATA/PARTDB/GAMEPART/ICB_CHAR.GCB", 36, catalog->headParts);
    CheckRetailLibrary(*vfs, "MENUDATA/PARTDB/MENUPART/CBBODIES.GCB", 73, catalog->headParts);
}

}
}
