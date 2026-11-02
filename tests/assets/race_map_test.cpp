#include "assets/race_map.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <format>
#include <optional>
#include <string>

#include "archive_survey.hpp"
#include "assets/token_bytes.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"

namespace rr::assets {

using test::Block;
using test::Bytes;
using test::Floats;
using test::Join;
using test::Text;

TEST_CASE("a race file gives its map world and bounds in that order") {
    const Result<RaceMapFields> fields =
        ReadRaceMapFields(Join({Bytes({0x3F}), Text("levc0r0"), Bytes({0x45}), Text("local.wdf"),
                                Bytes({0x46}), Floats({-700, 700, 800, -800})}));

    REQUIRE(fields.has_value());
    CHECK(fields->world == "local.wdf");
    REQUIRE(fields->bounds.has_value());
    const MapBounds bounds = fields->bounds.value_or(MapBounds{});
    CHECK(bounds.minX == -700.0F);
    CHECK(bounds.maxY == 700.0F);
    CHECK(bounds.maxX == 800.0F);
    CHECK(bounds.minY == -800.0F);
}

TEST_CASE("map fields inside a block still count") {
    const Result<RaceMapFields> fields =
        ReadRaceMapFields(Block(Join({Bytes({0x46}), Floats({-1, 2, 3, -4})})));

    REQUIRE(fields.has_value());
    CHECK_FALSE(fields->world.has_value());
    CHECK(fields->bounds.value_or(MapBounds{}).minY == -4.0F);
}

TEST_CASE("a race file without map fields has no map") {
    const Result<RaceMapFields> fields = ReadRaceMapFields(Join({Bytes({0x3F}), Text("test")}));

    REQUIRE(fields.has_value());
    CHECK_FALSE(fields->bounds.has_value());
    CHECK_FALSE(fields->world.has_value());
}

TEST_CASE("map bounds short of four numbers fail") {
    CHECK_FALSE(ReadRaceMapFields(Join({Bytes({0x46}), Floats({1, 2, 3})})).has_value());
}

namespace {

void CheckTrackMap(const Vfs& vfs, const std::string& track) {
    CAPTURE(track);
    const Result<RaceMapFields> fields =
        vfs.Read(std::format("GAMEDATA/{}/{}.RAB", track, track)).and_then([](const FileData& rab) {
            return ReadRaceMapFields(rab.Bytes());
        });
    REQUIRE(fields.has_value());
    CHECK(fields->world.has_value());
    REQUIRE(fields->bounds.has_value());
    const MapBounds bounds = fields->bounds.value_or(MapBounds{});
    CHECK(bounds.minX < bounds.maxX);
    CHECK(bounds.minY < bounds.maxY);
}

}

TEST_CASE("every retail race has a map") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    for (const std::string& track : ListRaceTracks(*vfs)) {
        CheckTrackMap(*vfs, track);
    }
}

}
