#include "app/racer/racer_models.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "assets/geometry_database.hpp"
#include "assets/merged_table.hpp"
#include "assets/part_library.hpp"
#include "assets/skeleton_database.hpp"
#include "assets/texture_database.hpp"
#include "sim/car_build/built_car.hpp"
#include "sim/car_build/car_mesh.hpp"
#include "sim/physics/racer_chassis.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kPartFolder = "MENUDATA/PARTDB";
constexpr std::uint8_t kPartGrey = 200;
constexpr std::int32_t kExpressionCount = 6;

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

Result<assets::TextureDefinition> DefaultFaceTexture(const assets::Vfs& vfs,
                                                     std::string_view name) {
    return assets::ReadMergedTable(vfs, std::format("{}/BODYPART.TDB", kPartFolder),
                                   assets::ParseTextureDatabase)
        .and_then([&](const std::vector<assets::TextureDefinition>& definitions)
                      -> Result<assets::TextureDefinition> {
            const std::string upper = assets::ToUpperAscii(name);
            const auto found =
                std::ranges::find_if(definitions, [&](const assets::TextureDefinition& definition) {
                    return assets::ToUpperAscii(definition.name) == upper;
                });
            if (found == definitions.end()) {
                return Fail(ErrorCode::NotFound, std::format("BODYPART.TDB has no {}", name));
            }
            return *found;
        });
}

}

Result<BuiltCarModel> BuildCarModel(const assets::Vfs& vfs, const assets::CarData& car,
                                    assets::PieceDetail detail) {
    Result<assets::PieceCatalog> pieces = assets::PieceCatalog::Load(vfs, detail);
    const Result<sim::BuiltCar> built = pieces.and_then([&](assets::PieceCatalog& catalog) {
        catalog.ReadExtraModels(vfs, assets::PieceTypesOf(car));
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
    return assets::ParseEveryLayer(vfs, std::format("{}/BODYPART.PCB", kPartFolder),
                                   assets::ParseDriverParts)
        .transform(assets::MergeDriverParts);
}

Result<assets::PartLibrary> LoadHeadParts(const assets::Vfs& vfs, std::string_view path) {
    return assets::ParseEveryLayer(vfs, path, assets::ParsePartLibrary)
        .transform(assets::MergePartLibraries);
}

Result<assets::GeometryAsset> BuildMinifig(const assets::Vfs& vfs,
                                           const assets::DriverPartCatalog& parts,
                                           const assets::DriverLook& look,
                                           assets::DriverDetail detail) {
    const MinifigFiles& files = detail == assets::DriverDetail::Race ? kRaceMinifig : kMenuMinifig;
    const Result<assets::PartLibrary> heads =
        LoadHeadParts(vfs, std::format("{}/{}", files.folder, files.heads));
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

Result<assets::SkeletonAsset> LoadMenuMinifigSkeleton(const assets::Vfs& vfs,
                                                      const assets::DriverPartCatalog& parts,
                                                      const assets::DriverLook& look) {
    return assets::DriverBodyName(parts, look, assets::DriverDetail::Menu)
        .and_then([&](const std::string& name) {
            return ReadParsed(
                vfs, std::format("{}/{}.SDB", kMenuMinifig.folder, assets::ToUpperAscii(name)),
                assets::ParseSkeleton);
        });
}

Result<std::string> MenuMinifigAnimations(const assets::DriverPartCatalog& parts,
                                          const assets::DriverLook& look) {
    return assets::DriverBodyName(parts, look, assets::DriverDetail::Menu)
        .transform([](const std::string& name) {
            return std::format("{}/{}.ADB", kMenuMinifig.folder, assets::ToUpperAscii(name));
        });
}

Result<assets::TrackAssets> LoadExpressionFaces(
    const assets::Vfs& vfs, const assets::NameMap<assets::MaterialAsset>& materials,
    std::string_view face) {
    const std::string defaultFace = assets::FaceMaterial(face, 0);
    const auto material = materials.find(assets::ToUpperAscii(defaultFace));
    if (material == materials.end()) {
        return Fail(ErrorCode::NotFound, std::format("there's no {} material", defaultFace));
    }
    const Result<assets::TextureDefinition> texture =
        DefaultFaceTexture(vfs, material->second.texture);
    if (!texture) {
        return std::unexpected(texture.error());
    }
    assets::TrackAssets faces;
    for (std::int32_t expression = 1; expression < kExpressionCount; ++expression) {
        assets::TextureDefinition definition = *texture;
        definition.name = assets::FaceMaterial(face, expression);
        Result<assets::ImageAsset> image =
            vfs.Read(std::format("{}/{}", kPartFolder, assets::ToUpperAscii(definition.FileName())))
                .and_then([&](const assets::FileData& data) {
                    return assets::DecodeTexture(data.Bytes(), definition);
                });
        if (!image) {
            return std::unexpected(std::move(image.error()));
        }
        assets::MaterialAsset shown = material->second;
        shown.name = definition.name;
        shown.texture = definition.name;
        const std::string key = assets::ToUpperAscii(definition.name);
        faces.materials.insert_or_assign(key, std::move(shown));
        faces.textures.insert_or_assign(
            key, std::make_shared<const assets::ImageAsset>(std::move(*image)));
    }
    return faces;
}

void GreyMinifigMaterials(assets::NameMap<assets::MaterialAsset>& materials,
                          const assets::DriverPartCatalog& parts, const assets::DriverLook& worn) {
    const assets::DriverLook look = assets::FitLook(parts, worn);
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
