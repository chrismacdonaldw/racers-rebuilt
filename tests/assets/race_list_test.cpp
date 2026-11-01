#include "assets/race_list.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Ints;
using test::Join;
using test::NamedEntry;
using test::NamedLibrary;
using test::Text;

std::vector<std::byte> Race(std::uint8_t position, std::string_view folder) {
    return Join({Bytes({0x28}), Ints({position}), Bytes({0x29}), Text(folder)});
}

TEST_CASE("races read their folder, circuit place, name line, mirroring and preview") {
    const std::vector rcb = NamedLibrary(
        0x27,
        {NamedEntry(0x27, "igp2",
                    Join({Bytes({0x2B}), Ints({5}), Race(3, "racec0r1"), Bytes({0x2A}), Text("c3"),
                          Bytes({0x2C, 0x2D}), Text("pirate1"), Bytes({0x2E}), Text("GB")})),
         NamedEntry(0x27, "test", Join({Bytes({0x29}), Text("test")}))});

    const Result<std::vector<RaceEntry>> races = ParseRaces(rcb);

    REQUIRE(races.has_value());
    REQUIRE(races->size() == 2);
    const RaceEntry& mirrored = races->front();
    CHECK(mirrored.name == "igp2");
    CHECK(mirrored.folder == "racec0r1");
    CHECK(mirrored.circuit == "c3");
    CHECK(mirrored.position == 3);
    CHECK(mirrored.nameLine == 5);
    CHECK(mirrored.isMirrored);
    CHECK(mirrored.previewScene == "pirate1");
    CHECK(mirrored.previewDriver == "GB");
    const RaceEntry& test = races->back();
    CHECK_FALSE(test.circuit.has_value());
    CHECK(test.position == 0);
    CHECK_FALSE(test.isMirrored);
}

TEST_CASE("a race needs a folder and a circuit position from 0 to 3") {
    const auto parses = [](const std::vector<std::byte>& body) {
        return ParseRaces(NamedLibrary(0x27, {NamedEntry(0x27, "igp", body)})).has_value();
    };

    CHECK(parses(Race(3, "racec0r1")));
    CHECK_FALSE(parses(Race(4, "racec0r1")));
    CHECK_FALSE(parses(Join({Bytes({0x28}), Ints({1})})));
}

TEST_CASE("the retail race list has 27 races") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<std::vector<RaceEntry>> races =
        vfs->Read("MENUDATA/LEGORACE.RCB").and_then([](const FileData& data) {
            return ParseRaces(data.Bytes());
        });

    REQUIRE(races.has_value());
    CHECK(races->size() == 27);
}

}
}
