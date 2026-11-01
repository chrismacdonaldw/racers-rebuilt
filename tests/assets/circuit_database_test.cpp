#include "assets/circuit_database.hpp"

#include <doctest/doctest.h>

#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Ints;
using test::Join;
using test::ListHeader;
using test::NamedEntry;
using test::NamedLibrary;
using test::Text;

std::vector<std::byte> Grid(std::initializer_list<std::string_view> codes) {
    std::vector<std::byte> grid =
        Join({Bytes({0x28}), ListHeader(static_cast<std::uint8_t>(codes.size()))});
    for (const std::string_view code : codes) {
        grid = Join({grid, Text(code)});
    }
    return Join({grid, Bytes({0x06})});
}

TEST_CASE("circuits read their grid drivers and the circuit they lead to") {
    const std::vector crb = NamedLibrary(
        0x27,
        {NamedEntry(0x27, "c0",
                    Join({Bytes({0x29}), Ints({0}), Grid({"KK", "CR", "GB", "RH", "AD", "PH"}),
                          Bytes({0x2A}), Ints({0}), Bytes({0x2B}), Text("c1")})),
         NamedEntry(0x27, "c6", Grid({"RR", "RR", "BK", "AL", "GS", "NH"}))});

    const Result<std::vector<CircuitEntry>> circuits = ParseCircuits(crb);

    REQUIRE(circuits.has_value());
    REQUIRE(circuits->size() == 2);
    CHECK(circuits->front().name == "c0");
    CHECK(circuits->front().grid == std::vector<std::string>{"KK", "CR", "GB", "RH", "AD", "PH"});
    CHECK(circuits->front().next == "c1");
    CHECK_FALSE(circuits->back().next.has_value());
}

TEST_CASE("a circuit may leave grid slots empty but needs a driver and no more than 6") {
    const auto parses = [](std::initializer_list<std::string_view> codes) {
        return ParseCircuits(NamedLibrary(0x27, {NamedEntry(0x27, "c0", Grid(codes))})).has_value();
    };

    CHECK(parses({"KK", "CR", "GB"}));
    CHECK_FALSE(parses({}));
    CHECK_FALSE(parses({"KK", "CR", "GB", "RH", "AD", "PH", "RR"}));
}

std::vector<std::string> UnknownDrivers(const RacerCatalog& catalog,
                                        std::span<const CircuitEntry> circuits) {
    std::vector<std::string> unknown;
    for (const CircuitEntry& circuit : circuits) {
        for (const std::string& code : circuit.grid) {
            if (!catalog.Find(code)) {
                unknown.push_back(std::format("{} {}", circuit.name, code));
            }
        }
    }
    return unknown;
}

TEST_CASE("every retail grid driver resolves to a racer") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<RacerCatalog> catalog = RacerCatalog::Load(*vfs);
    const Result<std::vector<CircuitEntry>> circuits =
        vfs->Read("MENUDATA/LEGORACE.CRB").and_then([](const FileData& data) {
            return ParseCircuits(data.Bytes());
        });
    REQUIRE(catalog.has_value());
    REQUIRE(circuits.has_value());

    CHECK(circuits->size() == 7);
    CHECK(UnknownDrivers(*catalog, *circuits).empty());
}

TEST_CASE("a track's grid comes from its circuit") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<std::vector<std::string>> grid = LoadGrid(*vfs, "racec4r2");

    REQUIRE(grid.has_value());
    CHECK(grid->at(1) == "BVB");
    CHECK_FALSE(LoadGrid(*vfs, "MENUDATA").has_value());
}

}
}
