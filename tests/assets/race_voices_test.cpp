#include "assets/race_voices.hpp"

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
using test::Join;
using test::Text;

TEST_CASE("a race names its opponents' voice bank inside its block") {
    const Result<std::optional<std::string>> voices =
        ReadOpponentVoices(Join({Bytes({0x35}), Text("Royal Knights Raceway"),
                                 Block(Join({Bytes({0x3C}), Text("general"), Bytes({0x3D}),
                                             Text("voiceC1"), Bytes({0x3F}), Text("levc0r0")}))}));

    REQUIRE(voices.has_value());
    CHECK(*voices == "voiceC1");
}

TEST_CASE("a race without opponent voices reads as none") {
    const Result<std::optional<std::string>> voices =
        ReadOpponentVoices(Join({Bytes({0x3F}), Text("test")}));

    REQUIRE(voices.has_value());
    CHECK_FALSE(voices->has_value());
}

TEST_CASE("an opponent voice field without a name fails") {
    CHECK_FALSE(ReadOpponentVoices(Join({Bytes({0x3D}), Bytes({0x0B, 0x01})})).has_value());
}

namespace {

void CheckTrackVoices(const Vfs& vfs, const std::string& track) {
    CAPTURE(track);
    const Result<FileData> rab = vfs.Read(std::format("GAMEDATA/{}/{}.RAB", track, track));
    REQUIRE(rab.has_value());
    const Result<std::optional<std::string>> voices = ReadOpponentVoices(rab->Bytes());
    REQUIRE(voices.has_value());
    CHECK(voices->value_or("").starts_with("voiceC"));
}

}

TEST_CASE("every retail circuit race names a circuit voice bank") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    for (const std::string& track : ListRaceTracks(*vfs)) {
        CheckTrackVoices(*vfs, track);
    }
}

}
