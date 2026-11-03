#include "assets/driver_parts.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Ints;
using test::Join;
using test::ListHeader;
using test::Text;

std::vector<std::byte> FieldList(std::uint8_t field, std::uint8_t count,
                                 const std::vector<std::byte>& body) {
    return Join({Bytes({field}), ListHeader(count), body, Bytes({0x06})});
}

std::vector<std::byte> SmallCatalog() {
    return Join({
        FieldList(0x28, 2, Join({Text("MENUPART"), Text("GAMEPART")})),
        FieldList(0x27, 1, Text("cbbodies.wdf")),
        FieldList(0x29, 2, Join({Text("nohat"), Ints({0}), Text("helmtvvs"), Ints({128})})),
        FieldList(0x2A, 1, Join({Text("heads"), Text("kk_"), Ints({4})})),
        FieldList(0x2B, 1, Join({Text("chests"), Text("chest_hk"), Text("kk_chst"), Ints({1, 4})})),
        FieldList(0x2C, 1, Join({Text("legss"), Text("leg-pegs"), Text("leg_peg"), Ints({1, 3})})),
        FieldList(0x2E, 2, Join({Text("rr"), Text("leg_box")})),
        FieldList(0x2D, 2, Join({Text("head"), Text("helmetvv")})),
        FieldList(0x2F, 1, Text("hat")),
    });
}

TEST_CASE("the part catalogue reads hats, faces, torsos, legs and their models") {
    const Result<DriverPartCatalog> catalog = ParseDriverParts(SmallCatalog());

    REQUIRE(catalog.has_value());
    CHECK(catalog->modelFolders == std::vector<std::string>{"MENUPART", "GAMEPART"});
    CHECK(catalog->modelFiles == std::vector<std::string>{"cbbodies.wdf"});
    REQUIRE(catalog->hats.size() == 2);
    CHECK(catalog->hats[1].name == "helmtvvs");
    CHECK(catalog->hats[1].unlock == 128);
    CHECK(catalog->faces.folder == "heads");
    REQUIRE(catalog->faces.parts.size() == 1);
    CHECK(catalog->faces.parts[0].name == "kk_");
    CHECK(catalog->faces.parts[0].unlock == 4);
    CHECK(catalog->torsos.folder == "chests");
    CHECK(catalog->torsos.hookFolder == "chest_hk");
    REQUIRE(catalog->torsos.parts.size() == 1);
    CHECK(catalog->torsos.parts[0].name == "kk_chst");
    CHECK(catalog->torsos.parts[0].variant == 1);
    CHECK(catalog->torsos.parts[0].unlock == 4);
    CHECK(catalog->legs.hookFolder == "leg-pegs");
    REQUIRE(catalog->legs.parts.size() == 1);
    CHECK(catalog->legs.parts[0].unlock == 3);
    CHECK(catalog->bodies == std::vector<std::string>{"rr", "leg_box"});
    CHECK(catalog->headParts == std::vector<std::string>{"head", "helmetvv"});
    CHECK(catalog->slots == std::vector<std::string>{"hat"});
}

TEST_CASE("a catalogue with a head part missing for a hat fails") {
    const std::vector<std::byte> pcb =
        Join({FieldList(0x29, 2, Join({Text("nohat"), Ints({0}), Text("bb_hats"), Ints({5})})),
              FieldList(0x2D, 1, Text("head"))});

    CHECK_FALSE(ParseDriverParts(pcb).has_value());
}

TEST_CASE("a catalogue with a repeated or unknown field fails") {
    CHECK_FALSE(
        ParseDriverParts(Join({SmallCatalog(), FieldList(0x2F, 1, Text("hat"))})).has_value());
    CHECK_FALSE(ParseDriverParts(FieldList(0x30, 1, Text("hat"))).has_value());
}

TEST_CASE("parts unlock always, by part set or once every race is unlocked") {
    const std::set<std::int32_t> partSets{0, 6};

    CHECK(IsPartUnlocked(0, {}, false));
    CHECK(IsPartUnlocked(2, {}, false));
    CHECK(IsPartUnlocked(3, partSets, false));
    CHECK_FALSE(IsPartUnlocked(4, partSets, false));
    CHECK(IsPartUnlocked(9, partSets, false));
    CHECK_FALSE(IsPartUnlocked(128, partSets, false));
    CHECK(IsPartUnlocked(128, {}, true));
}

TEST_CASE("the expression picks the face material suffix") {
    CHECK(FaceMaterial("kk_", 0) == "kk_dflt");
    CHECK(FaceMaterial("kk_", 1) == "kk_angry");
    CHECK(FaceMaterial("kk_", 2) == "kk_blink");
    CHECK(FaceMaterial("kk_", 3) == "kk_happy");
    CHECK(FaceMaterial("kk_", 4) == "kk_sad");
    CHECK(FaceMaterial("kk_", 5) == "kk_suprz");
    CHECK(FaceMaterial("kk_", 6) == "kk_dflt");
    CHECK(FaceMaterial("kk_", -1) == "kk_dflt");
}

TEST_CASE("a bare expression suffix names its expression in any case") {
    CHECK(FaceExpression("dflt") == 0);
    CHECK(FaceExpression("angry") == 1);
    CHECK(FaceExpression("Blink") == 2);
    CHECK(FaceExpression("HAPPY") == 3);
    CHECK(FaceExpression("sad") == 4);
    CHECK(FaceExpression("suprz") == 5);
    CHECK(FaceExpression("lh_happy") == 0);
    CHECK(FaceExpression("rblink") == 0);
}

std::vector<std::size_t> ListSizes(const DriverPartCatalog& catalog) {
    return {catalog.hats.size(),       catalog.faces.parts.size(), catalog.torsos.parts.size(),
            catalog.legs.parts.size(), catalog.headParts.size(),   catalog.bodies.size(),
            catalog.slots.size()};
}

std::vector<std::string> Folders(const DriverPartCatalog& catalog) {
    return {catalog.faces.folder, catalog.torsos.folder, catalog.torsos.hookFolder,
            catalog.legs.folder, catalog.legs.hookFolder};
}

TEST_CASE("the retail part catalogue lists every part the save can index") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<DriverPartCatalog> catalog =
        vfs->Read("MENUDATA/PARTDB/BODYPART.PCB").and_then([](const FileData& data) {
            return ParseDriverParts(data.Bytes());
        });

    REQUIRE(catalog.has_value());
    CHECK(ListSizes(*catalog) == std::vector<std::size_t>{36, 30, 29, 20, 36, 5, 4});
    CHECK(Folders(*catalog) ==
          std::vector<std::string>{"heads", "chests", "chest_hk", "legss", "leg-pegs"});
}

DriverPartCatalog Shelves(std::vector<DriverPart> hats, std::vector<std::string> headParts,
                          std::vector<DriverPart> torsos) {
    DriverPartCatalog catalog;
    catalog.hats = std::move(hats);
    catalog.headParts = std::move(headParts);
    catalog.torsos.parts = std::move(torsos);
    return catalog;
}

std::vector<std::string> Names(const std::vector<DriverPart>& parts) {
    std::vector<std::string> names;
    names.reserve(parts.size());
    for (const DriverPart& part : parts) {
        names.push_back(part.name);
    }
    return names;
}

TEST_CASE("a mod's parts keep the game's indices and its hats bring their head parts") {
    DriverPartCatalog game =
        Shelves({{.name = "nohat"}, {.name = "cap"}}, {"head", "capped"}, {{.name = "kk_chst"}});
    game.torsos.folder = "chests";
    game.bodies = {"rr"};
    const DriverPartCatalog mod =
        Shelves({{.name = "CAP", .unlock = 3}, {.name = "crown"}}, {"recapped", "crowned"},
                {{.name = "robe", .variant = 1}});

    const DriverPartCatalog merged = MergeDriverParts({game, mod});

    CHECK(Names(merged.hats) == std::vector<std::string>{"nohat", "CAP", "crown"});
    CHECK(merged.hats[1].unlock == 3);
    CHECK(merged.headParts == std::vector<std::string>{"head", "recapped", "crowned"});
    CHECK(Names(merged.torsos.parts) == std::vector<std::string>{"kk_chst", "robe"});
    CHECK(merged.torsos.folder == "chests");
    CHECK(merged.bodies == std::vector<std::string>{"rr"});
}

}
}
