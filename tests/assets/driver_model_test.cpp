#include "assets/driver_model.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/game_path.hpp"
#include "assets/geometry_database.hpp"
#include "assets/material_library.hpp"
#include "assets/original_save.hpp"

namespace rr::assets {
namespace {

DriverPartCatalog SmallCatalog() {
    return {
        .hats = {{.name = "nohat"}, {.name = "kk_hats"}},
        .faces = {.folder = "heads", .parts = {{.name = "lh_"}, {.name = "kk_"}}},
        .torsos = {.parts = {{.name = "tu_chst"}, {.name = "vb_chst", .variant = 1}}},
        .legs = {.parts = {{.name = "leg_blck"}, {.name = "leg_peg", .variant = 1}}},
        .headParts = {"head", "kk_hat"},
        .bodies = {"rr", "hr", "rp", "hp", "leg_box"},
    };
}

PartLibrary SmallHeads() {
    return {
        .vertices = {{.position = {.x = 2.0F}}, {.position = {.y = 4.0F}}, {}},
        .parts = {{.name = "head", .groups = {{.material = "face", .triangles = {{0, 1, 2}}}}},
                  {.name = "kk_hat",
                   .groups = {{.material = "face", .triangles = {{0, 1, 2}}},
                              {.material = "kk_hat", .triangles = {{2, 1, 0}}}}}},
    };
}

GeometryAsset SmallBody() {
    return {
        .vertices = {{}, {}, {}, {}},
        .indices = {0, 1, 2, 1, 2, 3},
        .materials = {"torso", "face", "legs"},
        .ranges = {{.firstIndex = 0, .indexCount = 3, .material = 0, .joint = 1},
                   {.firstIndex = 3, .indexCount = 3, .material = 1, .joint = 4}},
        .scale = 0.5F,
    };
}

TEST_CASE("menu bodies follow the torso and legs variants and races use leg_box") {
    const DriverPartCatalog catalog = SmallCatalog();

    CHECK(DriverBodyName(catalog, {}, DriverDetail::Menu) == "rr");
    CHECK(DriverBodyName(catalog, {.torso = 1}, DriverDetail::Menu) == "hr");
    CHECK(DriverBodyName(catalog, {.legs = 1}, DriverDetail::Menu) == "rp");
    CHECK(DriverBodyName(catalog, {.legs = 1, .torso = 1}, DriverDetail::Menu) == "hp");
    CHECK(DriverBodyName(catalog, {.legs = 1, .torso = 1}, DriverDetail::Race) == "leg_box");
}

Result<GeometryAsset> HattedMenuDriver() {
    return BuildDriverGeometry(SmallCatalog(), SmallHeads(), SmallBody(),
                               {.face = 1, .hat = 1, .legs = 1, .torso = 1, .expression = 3},
                               DriverDetail::Menu);
}

std::vector<std::uint16_t> Joints(const GeometryAsset& geometry) {
    return geometry.vertices | std::views::transform(&GeometryVertex::joint) |
           std::ranges::to<std::vector>();
}

TEST_CASE("a driver merges its head onto the face joint at the body's scale") {
    const Result<GeometryAsset> driver = HattedMenuDriver();

    REQUIRE(driver.has_value());
    CHECK(Joints(*driver) == std::vector<std::uint16_t>{0, 0, 0, 0, 4, 4, 4});
    CHECK(driver->vertices.at(5).position.y == 2.0F);
    CHECK(driver->indices == std::vector<std::uint32_t>{0, 1, 2, 1, 2, 3, 4, 5, 6, 6, 5, 4});
}

TEST_CASE("a menu driver shares materials by name and wears its parts and expression") {
    const Result<GeometryAsset> driver = HattedMenuDriver();

    REQUIRE(driver.has_value());
    CHECK(driver->materials ==
          std::vector<std::string>{"vb_chst", "kk_happy", "leg_peg", "kk_hat"});
    CHECK(driver->ranges == std::vector<GeometryRange>{
                                {.firstIndex = 0, .indexCount = 3, .material = 0, .joint = 1},
                                {.firstIndex = 3, .indexCount = 3, .material = 1, .joint = 4},
                                {.firstIndex = 6, .indexCount = 3, .material = 1, .joint = 4},
                                {.firstIndex = 9, .indexCount = 3, .material = 3, .joint = 4},
                            });
}

TEST_CASE("a race driver shows the default face") {
    const Result<GeometryAsset> driver =
        BuildDriverGeometry(SmallCatalog(), SmallHeads(), SmallBody(), {.face = 1, .expression = 3},
                            DriverDetail::Race);

    REQUIRE(driver.has_value());
    CHECK(driver->materials == std::vector<std::string>{"tu_chst", "kk_dflt", "leg_blck"});
}

TEST_CASE("a look past the catalogue or a body without a face fails") {
    const DriverPartCatalog catalog = SmallCatalog();
    const PartLibrary heads = SmallHeads();
    const GeometryAsset body = SmallBody();

    CHECK_FALSE(
        BuildDriverGeometry(catalog, heads, body, {.face = 2}, DriverDetail::Menu).has_value());
    CHECK_FALSE(
        BuildDriverGeometry(catalog, heads, body, {.hat = 2}, DriverDetail::Menu).has_value());
    CHECK_FALSE(
        BuildDriverGeometry(catalog, heads, body, {.legs = 2}, DriverDetail::Menu).has_value());
    CHECK_FALSE(
        BuildDriverGeometry(catalog, heads, body, {.torso = 2}, DriverDetail::Menu).has_value());
    CHECK_FALSE(DriverBodyName(catalog, {.torso = 2}, DriverDetail::Menu).has_value());

    GeometryAsset faceless = body;
    faceless.materials[1] = "head";
    CHECK_FALSE(BuildDriverGeometry(catalog, heads, faceless, {}, DriverDetail::Menu).has_value());
}

template <typename T, typename Parse>
Result<T> ReadRetail(const Vfs& vfs, const std::string& path, Parse parse) {
    return vfs.Read(path).and_then([&](const FileData& data) { return parse(data.Bytes()); });
}

struct RetailParts {
    DriverPartCatalog catalog;
    std::vector<std::string> materials;
};

RetailParts LoadRetailParts(const Vfs& vfs) {
    const Result<DriverPartCatalog> catalog =
        ReadRetail<DriverPartCatalog>(vfs, "MENUDATA/PARTDB/BODYPART.PCB", ParseDriverParts);
    const Result<std::vector<MaterialAsset>> materials = ReadRetail<std::vector<MaterialAsset>>(
        vfs, "MENUDATA/PARTDB/BODYPART.MDB", ParseMaterialLibrary);
    REQUIRE(catalog.has_value());
    REQUIRE(materials.has_value());
    return {
        .catalog = *catalog,
        .materials = *materials | std::views::transform(&MaterialAsset::name) |
                     std::ranges::to<std::vector>(),
    };
}

Result<GeometryAsset> BuildRetailDriver(const Vfs& vfs, const DriverPartCatalog& catalog,
                                        const DriverLook& look, DriverDetail detail) {
    const bool isRace = detail == DriverDetail::Race;
    const std::string folder = isRace ? "MENUDATA/PARTDB/GAMEPART/" : "MENUDATA/PARTDB/MENUPART/";
    const Result<PartLibrary> heads = ReadRetail<PartLibrary>(
        vfs, folder + (isRace ? "ICB_CHAR.GCB" : "CBBODIES.GCB"), ParsePartLibrary);
    return DriverBodyName(catalog, look, detail)
        .and_then([&](const std::string& body) {
            return ReadRetail<GeometryAsset>(vfs, folder + ToUpperAscii(body) + ".GDB",
                                             ParseGeometry);
        })
        .and_then([&](const GeometryAsset& body) {
            return heads.and_then([&](const PartLibrary& library) {
                return BuildDriverGeometry(catalog, library, body, look, detail);
            });
        });
}

std::vector<std::string> UnknownMaterials(const Vfs& vfs, const RetailParts& parts,
                                          const GeometryAsset& driver) {
    return driver.materials | std::views::filter([&](const std::string& material) {
               return !std::ranges::contains(parts.materials, material) &&
                      !vfs.Read("MENUDATA/PARTDB/" + ToUpperAscii(material) + ".BMP");
           }) |
           std::ranges::to<std::vector>();
}

void CheckRetailDriver(const Vfs& vfs, const RetailParts& parts, const SavedRacer& racer,
                       DriverDetail detail) {
    const Result<GeometryAsset> driver = BuildRetailDriver(vfs, parts.catalog, racer.look, detail);

    REQUIRE(driver.has_value());
    const std::int32_t expression = detail == DriverDetail::Race ? 0 : racer.look.expression;
    CHECK(std::ranges::contains(
        driver->materials,
        FaceMaterial(parts.catalog.faces.parts.at(racer.look.face).name, expression)));
    CHECK(UnknownMaterials(vfs, parts, *driver).empty());
}

std::vector<std::u16string> Names(const std::vector<SavedRacer>& racers) {
    return racers | std::views::transform(&SavedRacer::name) | std::ranges::to<std::vector>();
}

TEST_CASE("the default racers build in menus and races from retail parts") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const RetailParts parts = LoadRetailParts(*vfs);
    const Result<std::vector<SavedRacer>> racers =
        ReadRetail<std::vector<SavedRacer>>(*vfs, "MENUDATA/DEFAULT.LRS", ParseRacerFile);
    REQUIRE(racers.has_value());
    CHECK(Names(*racers) ==
          std::vector<std::u16string>{u"JOAN OF KART", u"TURBO CHARGER", u"SCOOTER", u"ROBORACER"});

    for (const SavedRacer& racer : *racers) {
        CheckRetailDriver(*vfs, parts, racer, DriverDetail::Menu);
        CheckRetailDriver(*vfs, parts, racer, DriverDetail::Race);
    }
}

}
}
