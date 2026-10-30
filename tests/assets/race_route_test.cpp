#include "assets/race_route.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <numeric>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "assets/jam_archive.hpp"
#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Floats;
using test::Ints;
using test::Join;

constexpr std::uint8_t kKindTwoDuration96 = (2 << 6) | 3;

std::vector<std::byte> Signed16(std::initializer_list<std::int16_t> values) {
    std::vector<std::byte> bytes;
    for (const std::int16_t value : values) {
        const auto bits = static_cast<std::uint16_t>(value);
        bytes.insert(bytes.end(), {std::byte{0x0D}, static_cast<std::byte>(bits & 0xFFU),
                                   static_cast<std::byte>(bits >> 8U)});
    }
    return bytes;
}

std::vector<std::byte> Point(std::int16_t x, std::int16_t y,
                             std::initializer_list<std::uint8_t> bytes) {
    return Join({Signed16({x, y}), Ints(bytes)});
}

std::vector<std::byte> Route(std::uint8_t loopPoint, const std::vector<std::byte>& lastPoint) {
    return Join({
        Bytes({0x29}),
        Floats({1, 2, 3}),
        Bytes({0x28}),
        Floats({0, 0, 0, 1}),
        Bytes({0x2A}),
        Floats({4, 5, 6}),
        Bytes({0x2B}),
        Floats({0, 0, 0, 1}),
        Bytes({0x2C}),
        Ints({96}),
        Bytes({0x2D}),
        Ints({loopPoint}),
        Bytes({0x27}),
        test::ListHeader(2),
        Point(512, -256, {240, 0, 0, 127, 127, 16, 8, kKindTwoDuration96}),
        lastPoint,
        Bytes({0x06}),
    });
}

std::vector<std::byte> PlainPoint() {
    return Point(0, 0, {0, 0, 0, 0, 127, 0, 0, 1});
}

TEST_CASE("a race route decodes its packed points") {
    const Result<RaceRoute> route = ParseRaceRoute(Route(0, PlainPoint()));

    REQUIRE(route.has_value());
    CHECK(route->startPosition == Vector3{1, 2, 3});
    CHECK(route->startRotation == Quaternion{});
    CHECK(route->loopPosition == Vector3{4, 5, 6});
    CHECK(route->loopTimeMs == 96);
    CHECK(route->loopPoint == 0);
    REQUIRE(route->points.size() == 2);
    const RoutePoint& point = route->points.front();
    CHECK(point.offset == Vector3{2, -1, -1});
    CHECK(point.rotation.w == doctest::Approx(0.70711));
    CHECK(point.rotation.z == doctest::Approx(-0.70711));
    CHECK(point.widthLeft == 2.0F);
    CHECK(point.widthRight == 1.0F);
    CHECK(point.kind == 2);
    CHECK(point.durationMs == 96);
    CHECK(route->points.back().durationMs == 32);
}

TEST_CASE("a race route needs a point after its loop point") {
    CHECK_FALSE(ParseRaceRoute(Route(1, PlainPoint())).has_value());
}

TEST_CASE("a race route rejects bad points") {
    const std::vector<std::byte> byteTooLarge =
        Join({Signed16({0, 0, 300}), Ints({0, 0, 0, 127, 0, 0, 1})});
    const std::vector<std::byte> noDuration = Point(0, 0, {0, 0, 0, 0, 127, 0, 0, 64});
    const std::vector<std::byte> noRotation = Point(0, 0, {0, 0, 0, 0, 0, 0, 0, 1});

    CHECK_FALSE(ParseRaceRoute(Route(0, byteTooLarge)).has_value());
    CHECK_FALSE(ParseRaceRoute(Route(0, noDuration)).has_value());
    CHECK_FALSE(ParseRaceRoute(Route(0, noRotation)).has_value());
}

TEST_CASE("a mirrored race route reflects across the XZ plane and swaps its widths") {
    const Result<RaceRoute> route = ParseRaceRoute(Route(0, PlainPoint()));
    REQUIRE(route.has_value());

    const RaceRoute mirrored = Mirrored(*route);

    CHECK(mirrored.startPosition == Vector3{1, -2, 3});
    CHECK(mirrored.loopPosition == Vector3{4, -5, 6});
    const RoutePoint& point = mirrored.points.front();
    CHECK(point.offset == Vector3{2, 1, -1});
    CHECK(point.rotation == Quaternion{.w = -route->points.front().rotation.w,
                                       .z = route->points.front().rotation.z});
    CHECK(point.widthLeft == 1.0F);
    CHECK(point.widthRight == 2.0F);
    CHECK(Mirrored(mirrored).points.front().rotation == route->points.front().rotation);
}

TEST_CASE("every race route in the retail game parses and loops on time") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    std::size_t pointCount = 0;
    const test::Survey routes = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "RRB"; },
        [&](std::span<const std::byte> bytes) {
            return ParseRaceRoute(bytes).transform([&](const RaceRoute& route) {
                pointCount += route.points.size();
                const std::span toLoop(route.points.data(), route.loopPoint + 1);
                CHECK(std::accumulate(toLoop.begin(), toLoop.end(), 0,
                                      [](int total, const RoutePoint& point) {
                                          return total + point.durationMs;
                                      }) == route.loopTimeMs);
            });
        });
    MESSAGE("RRB: ", routes.fileCount, " files, ", pointCount, " points");
    test::CheckSurvey(routes, 208);
}

}
}
