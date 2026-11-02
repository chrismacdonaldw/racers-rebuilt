#include "assets/racer_catalog.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "archive_survey.hpp"
#include "assets/folder_layer.hpp"
#include "assets/token_bytes.hpp"
#include "temp_folder.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Join;
using test::NamedEntry;
using test::NamedLibrary;
using test::Text;

void WriteBytes(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

class CatalogFixture {
public:
    CatalogFixture() {
        const std::filesystem::path common = folder_.Path() / "GAMEDATA" / "COMMON";
        WriteBytes(common / "DRIVERS.DDB",
                   NamedLibrary(0x27, {NamedEntry(0x27, "RR", Join({Bytes({0x2B}), Text("rr")})),
                                       NamedEntry(0x27, "XX", Join({Bytes({0x2B}), Text("xx")}))}));
        WriteBytes(
            common / "CHAMPS.CCB",
            NamedLibrary(0x27, {NamedEntry(0x27, "rr", Join({Bytes({0x2B}), Text("rrchas0")}))}));
        WriteBytes(common / "CHASSIS.CMB",
                   NamedLibrary(
                       0x27, {NamedEntry(0x27, "rrchas0", Join({Bytes({0x39}), Text("shadow")}))}));
        Result<FolderLayer> layer = FolderLayer::Scan(folder_.Path());
        REQUIRE(layer.has_value());
        vfs_.Mount(std::make_unique<FolderLayer>(std::move(*layer)));
    }

    [[nodiscard]] const Vfs& Files() const { return vfs_; }

private:
    test::TempFolder folder_;
    Vfs vfs_;
};

TEST_CASE("a driver code leads to its champion and chassis") {
    const CatalogFixture fixture;
    const Result<RacerCatalog> catalog = RacerCatalog::Load(fixture.Files());
    REQUIRE(catalog.has_value());

    const Result<RacerFiles> racer = catalog->Find("rr");

    REQUIRE(racer.has_value());
    CHECK(racer->driver.name == "RR");
    CHECK(racer->champion.name == "rr");
    CHECK(racer->chassis.shadowTexture == "shadow");
    CHECK(catalog->Drivers().size() == 2);
}

TEST_CASE("a driver whose champion is missing fails") {
    const CatalogFixture fixture;
    const Result<RacerCatalog> catalog = RacerCatalog::Load(fixture.Files());
    REQUIRE(catalog.has_value());

    CHECK_FALSE(catalog->Find("XX").has_value());
    CHECK_FALSE(catalog->Find("ZZ").has_value());
}

TEST_CASE("a built racer's chassis is found by name in any case") {
    const CatalogFixture fixture;
    const Result<RacerCatalog> catalog = RacerCatalog::Load(fixture.Files());
    REQUIRE(catalog.has_value());

    const Result<ChassisEntry> chassis = catalog->FindChassis("RRCHAS0");

    REQUIRE(chassis.has_value());
    CHECK(chassis->shadowTexture == "shadow");
    CHECK_FALSE(catalog->FindChassis("dachas0").has_value());
}

TEST_CASE("every retail driver leads to a champion and a chassis") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<RacerCatalog> catalog = RacerCatalog::Load(*vfs);
    REQUIRE(catalog.has_value());

    std::vector<std::string> broken;
    for (const DriverEntry& driver : catalog->Drivers()) {
        if (!catalog->Find(driver.name)) {
            broken.push_back(driver.name);
        }
    }

    CHECK(catalog->Drivers().size() == 24);
    CHECK(broken.empty());
}

}
}
