#include "assets/loading_screen.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "archive_survey.hpp"
#include "assets/token_bytes.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"

namespace rr::assets {

using test::Block;
using test::Bytes;
using test::Floats;
using test::Ints;
using test::Join;
using test::ListHeader;
using test::Text;

TEST_CASE("a loading screen names its picture, its track name line and its ticks") {
    const Result<LoadingScreenAsset> screen = ParseLsb(
        Join({Bytes({0x27}),
              Block(Join({Bytes({0x28}), Text("psp"), Bytes({0x2A}), Ints({9}), Bytes({0x29}),
                          ListHeader(2), Floats({1, 2, 3, 4}), Bytes({0x06})}))}));

    REQUIRE(screen.has_value());
    CHECK(screen->image == "psp");
    CHECK(screen->nameIndex == 9);
    REQUIRE(screen->ticks.size() == 2);
    CHECK(screen->ticks[0][0] == 1.0F);
    CHECK(screen->ticks[1][1] == 4.0F);
}

TEST_CASE("a loading screen with an unknown field fails") {
    CHECK_FALSE(
        ParseLsb(Join({Bytes({0x27}), Block(Join({Bytes({0x2B}), Ints({1})}))})).has_value());
}

TEST_CASE("a loading screen with a short tick list fails") {
    CHECK_FALSE(ParseLsb(Join({Bytes({0x27}), Block(Join({Bytes({0x29}), ListHeader(2),
                                                          Floats({1, 2, 3}), Bytes({0x06})}))}))
                    .has_value());
}

namespace {

void CheckTrackScreen(const Vfs& vfs, const std::string& track) {
    CAPTURE(track);
    const Result<LoadingScreenAsset> screen =
        vfs.Read(std::format("GAMEDATA/{}/LOADSCRN.LSB", track)).and_then([](const FileData& lsb) {
            return ParseLsb(lsb.Bytes());
        });
    REQUIRE(screen.has_value());
    CHECK(vfs.Read(std::format("GAMEDATA/{}/{}.BMP", track, screen->image)).has_value());
    CHECK(screen->ticks.size() == 20);
    CHECK(std::ranges::all_of(screen->ticks, [](const auto& tick) {
        const auto& [x, y] = tick;
        return x > 0.0F && x < 1.0F && y > 0.0F && y < 1.0F;
    }));
}

}

TEST_CASE("every retail race has a loading screen") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    for (const std::string& track : ListRaceTracks(*vfs)) {
        CheckTrackScreen(*vfs, track);
    }
}

}
