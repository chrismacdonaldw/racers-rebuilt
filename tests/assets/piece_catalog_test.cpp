#include "assets/piece_catalog.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include "archive_survey.hpp"

namespace rr::assets {
namespace {

struct ExpectedLibrary {
    PieceDetail detail = PieceDetail::Menu;
    std::size_t indices = 0;
    std::size_t positions = 0;
    std::size_t normals = 0;
    std::size_t texCoords = 0;
};

struct ExpectedSet {
    std::string_view chassisName;
    std::uint16_t chassisType = 0;
    std::size_t choices = 0;
};

constexpr std::array kLibraries{
    ExpectedLibrary{.detail = PieceDetail::Menu,
                    .indices = 40790,
                    .positions = 2735,
                    .normals = 1334,
                    .texCoords = 777},
    ExpectedLibrary{.detail = PieceDetail::Race,
                    .indices = 29057,
                    .positions = 2000,
                    .normals = 979,
                    .texCoords = 547},
};

constexpr std::array kSets{
    ExpectedSet{.chassisName = "ddchas0", .chassisType = 20, .choices = 30},
    ExpectedSet{.chassisName = "dbchas0", .chassisType = 18, .choices = 29},
    ExpectedSet{.chassisName = "dcchas0", .chassisType = 19, .choices = 24},
    ExpectedSet{.chassisName = "dachas0", .chassisType = 17, .choices = 20},
    ExpectedSet{.chassisName = "crchas0", .chassisType = 15, .choices = 20},
    ExpectedSet{.chassisName = "kkchas0", .chassisType = 16, .choices = 19},
    ExpectedSet{.chassisName = "bbchas0", .chassisType = 14, .choices = 15},
    ExpectedSet{.chassisName = "jtchas0", .chassisType = 11, .choices = 19},
    ExpectedSet{.chassisName = "bvbcha0", .chassisType = 13, .choices = 16},
    ExpectedSet{.chassisName = "gm_chas0", .chassisType = 12, .choices = 21},
    ExpectedSet{.chassisName = "rrchas0", .chassisType = 21, .choices = 13},
    ExpectedSet{.chassisName = "vvchas0", .chassisType = 22, .choices = 14},
};

std::array<std::size_t, 5> Sizes(const PieceLibrary& library) {
    return {library.pieces.size(), library.indices.size(), library.positions.size(),
            library.normals.size(), library.texCoords.size()};
}

void CheckLibrary(const PieceCatalog& catalog, const ExpectedLibrary& expected) {
    CHECK(Sizes(catalog.library) == std::array<std::size_t, 5>{161, expected.indices,
                                                               expected.positions, expected.normals,
                                                               expected.texCoords});
    CHECK(catalog.colours.size() == 10);
    CHECK(catalog.colours.at(2) == "biege");
    CHECK(catalog.library.Find(kStudPieceType, 1)->name == "Cylinder");
}

void CheckChassis(const PieceRecord& chassis, const ExpectedSet& expected) {
    CHECK(chassis.type == expected.chassisType);
    CHECK(chassis.shape.size == CellSize{.width = 10, .height = 6});
    CHECK(chassis.modelOffsetPosition.has_value());
}

void CheckPartSet(const PieceLibrary& library, const CatalogPartSet& set,
                  const ExpectedSet& expected) {
    CHECK(set.chassisName == expected.chassisName);
    CHECK(set.choices.size() == expected.choices);
    CheckChassis(library.pieces.at(set.chassis), expected);
}

void CheckPartSets(const PieceCatalog& catalog) {
    REQUIRE(catalog.partSets.size() == kSets.size());
    for (std::size_t i = 0; i < kSets.size(); ++i) {
        CheckPartSet(catalog.library, catalog.partSets[i], kSets.at(i));
    }
}

}

TEST_CASE("a listed set file swaps its text extension for the binary one") {
    CHECK(BinaryPartSetFile("dp_cset.leg") == "DP_CSET.LEB");
    CHECK(BinaryPartSetFile("bvb_cset") == "BVB_CSET.LEB");
}

TEST_CASE("both retail piece catalogues resolve every part set choice") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    for (const ExpectedLibrary& expected : kLibraries) {
        const Result<PieceCatalog> catalog = PieceCatalog::Load(*vfs, expected.detail);
        REQUIRE_MESSAGE(catalog.has_value(), catalog.error().message);
        CheckLibrary(*catalog, expected);
        CheckPartSets(*catalog);
    }
}

}
