#include "app/racer/racer_models.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/geometry_database.hpp"
#include "assets/part_library.hpp"
#include "sim/car_build/built_car.hpp"
#include "sim/car_build/car_mesh.hpp"
#include "sim/physics/racer_chassis.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kPartFolder = "MENUDATA/PARTDB";
constexpr std::uint8_t kPartGrey = 200;

struct MinifigFiles {
    std::string_view folder;
    std::string_view heads;
};

constexpr MinifigFiles kMenuMinifig{.folder = "MENUDATA/PARTDB/MENUPART", .heads = "CBBODIES.GCB"};
constexpr MinifigFiles kRaceMinifig{.folder = "MENUDATA/PARTDB/GAMEPART", .heads = "ICB_CHAR.GCB"};

template <typename T>
Result<T> ReadParsed(const assets::Vfs& vfs, const std::string& path,
                     Result<T> (*parse)(std::span<const std::byte>)) {
    return vfs.Read(path).and_then(
        [parse](const assets::FileData& data) { return parse(data.Bytes()); });
}

}

Result<BuiltCarModel> BuildCarModel(const assets::Vfs& vfs, const assets::CarData& car,
                                    assets::PieceDetail detail) {
    const Result<assets::PieceCatalog> pieces = assets::PieceCatalog::Load(vfs, detail);
    const Result<sim::BuiltCar> built = pieces.and_then([&](const assets::PieceCatalog& catalog) {
        return sim::BuiltCar::Load(car, catalog.library);
    });
    return built
        .and_then([&](const sim::BuiltCar& loaded) { return sim::BuildCarMesh(loaded, *pieces); })
        .transform([&](sim::CarMesh mesh) {
            return BuiltCarModel{.geometry = std::move(mesh.geometry),
                                 .build = sim::BuiltCarBuild(built->Bricks())};
        });
}

Result<assets::DriverPartCatalog> LoadDriverParts(const assets::Vfs& vfs) {
    return ReadParsed(vfs, std::format("{}/BODYPART.PCB", kPartFolder), assets::ParseDriverParts);
}

Result<assets::GeometryAsset> BuildMinifig(const assets::Vfs& vfs,
                                           const assets::DriverPartCatalog& parts,
                                           const assets::DriverLook& look,
                                           assets::DriverDetail detail) {
    const MinifigFiles& files = detail == assets::DriverDetail::Race ? kRaceMinifig : kMenuMinifig;
    const Result<assets::PartLibrary> heads =
        ReadParsed(vfs, std::format("{}/{}", files.folder, files.heads), assets::ParsePartLibrary);
    const Result<assets::GeometryAsset> body =
        heads
            .and_then([&](const assets::PartLibrary&) {
                return assets::DriverBodyName(parts, look, detail);
            })
            .and_then([&](const std::string& name) {
                return ReadParsed(
                    vfs, std::format("{}/{}.GDB", files.folder, assets::ToUpperAscii(name)),
                    assets::ParseGeometry);
            });
    return body.and_then([&](const assets::GeometryAsset& geometry) {
        return assets::BuildDriverGeometry(parts, *heads, geometry, look, detail);
    });
}

void GreyMinifigMaterials(assets::NameMap<assets::MaterialAsset>& materials,
                          const assets::DriverPartCatalog& parts, const assets::DriverLook& look) {
    const std::array names{assets::FaceMaterial(parts.faces.parts.at(look.face).name, 0),
                           parts.torsos.parts.at(look.torso).name,
                           parts.legs.parts.at(look.legs).name};
    for (const std::string& name : names) {
        const auto material = materials.find(assets::ToUpperAscii(name));
        if (material == materials.end()) {
            continue;
        }
        for (Rgba8* color : {&material->second.ambient, &material->second.diffuse}) {
            *color = {kPartGrey, kPartGrey, kPartGrey, (*color)[3]};
        }
    }
}

}
