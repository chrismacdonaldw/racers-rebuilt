#include "sim/car_build/car_mesh.hpp"

#include <algorithm>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "sim/car_build/car_studs.hpp"
#include "sim/car_build/hidden_faces.hpp"
#include "sim/car_build/mesh_primitives.hpp"
#include "sim/car_build/placed_model.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kPieceVariant = 1;
constexpr std::uint16_t kCulledCommand = 1;
constexpr float kPlateHeight = 0.4F;
constexpr float kNormalUnit = 127.0F;
constexpr Vector3 kBareOffset{.x = -5.0F, .y = -3.0F};
constexpr std::uint32_t kUnused = std::numeric_limits<std::uint32_t>::max();

struct ModelInstance {
    const assets::GeometryAsset* model = nullptr;
    PieceInstance instance;
};

struct MeshInput {
    const assets::PieceLibrary* library = nullptr;
    std::vector<PieceInstance> pieces;
    std::vector<PieceInstance> studs;
    std::vector<ModelInstance> models;
};

Result<std::vector<std::uint32_t>> ColourMaterials(const assets::PieceCatalog& catalog) {
    std::vector<std::uint32_t> materials;
    for (const std::string& colour : catalog.colours) {
        const auto found = std::ranges::find(catalog.materials, colour);
        if (found == catalog.materials.end()) {
            return Fail(ErrorCode::NotFound, std::format("no material for colour {}", colour));
        }
        materials.push_back(static_cast<std::uint32_t>(found - catalog.materials.begin()));
    }
    return materials;
}

Result<std::uint32_t> MaterialOfColour(std::span<const std::uint32_t> materials,
                                       std::uint8_t colour) {
    if (colour >= materials.size()) {
        return Fail(ErrorCode::InvalidData, std::format("colour {} isn't in the list", colour));
    }
    return materials[colour];
}

std::uint8_t StudColour(const CarStud& stud, std::span<const std::uint32_t> materials) {
    if (stud.raisedBottom == 0) {
        return stud.colour;
    }
    const auto found = std::ranges::find(materials, static_cast<std::uint32_t>(stud.raisedBottom));
    return found == materials.end() ? 0 : static_cast<std::uint8_t>(found - materials.begin());
}

const assets::GeometryAsset* ModelOf(const assets::PieceRecord& record,
                                     const assets::PieceCatalog& catalog) {
    return record.IsChassis() ? nullptr : catalog.ModelOf(record);
}

void GatherStuds(const BuiltCar& car, const assets::PieceCatalog& catalog,
                 std::span<const std::uint32_t> materials, std::span<const std::uint8_t> isModelled,
                 MeshInput& input) {
    const assets::PieceRecord* stud = catalog.library.Find(assets::kStudPieceType, kPieceVariant);
    if (stud == nullptr) {
        return;
    }
    for (const CarStud& placed : CarStuds(car)) {
        if (isModelled[placed.topPiece] != 0) {
            continue;
        }
        input.studs.push_back({.piece = stud,
                               .placement = {.x = placed.x, .y = placed.y},
                               .height = placed.height,
                               .colourMaterial = materials[StudColour(placed, materials)]});
    }
}

Result<MeshInput> GatherInput(const BuiltCar& car, const assets::PieceCatalog& catalog) {
    Result<std::vector<std::uint32_t>> materials = ColourMaterials(catalog);
    if (!materials) {
        return std::unexpected(std::move(materials.error()));
    }
    MeshInput input{.library = &catalog.library};
    std::vector<std::uint8_t> isModelled;
    for (const CarPiece& piece : car.Pieces()) {
        const assets::PieceRecord* record = catalog.library.Find(piece.piece->type, kPieceVariant);
        const Result<std::uint32_t> colour = MaterialOfColour(*materials, piece.colour);
        if (record == nullptr || !colour) {
            return Fail(ErrorCode::InvalidData,
                        std::format("can't draw piece {}", piece.piece->name));
        }
        const PieceInstance instance{.piece = record,
                                     .placement = piece.placement,
                                     .height = piece.height,
                                     .colourMaterial = *colour,
                                     .part = input.pieces.size()};
        const assets::GeometryAsset* model = ModelOf(*record, catalog);
        isModelled.push_back(model != nullptr ? 1 : 0);
        if (model != nullptr) {
            input.models.push_back({.model = model, .instance = instance});
        } else {
            input.pieces.push_back(instance);
        }
    }
    GatherStuds(car, catalog, *materials, isModelled, input);
    return input;
}

void Finish(MeshPrimitives& primitives) {
    RemoveTouchingFaces(primitives);
    for (std::size_t slot = 0; slot < primitives.Live() && !primitives.HasOverflowed();) {
        if (primitives.At(slot).command == kCulledCommand) {
            primitives.Drop(slot);
        } else {
            ++slot;
        }
    }
    if (!primitives.HasOverflowed()) {
        primitives.SplitQuads();
    }
    if (!primitives.HasOverflowed()) {
        primitives.Sort();
    }
}

Result<void> BuildPass(const MeshInput& input, StudStyle studs, std::size_t pieceCount,
                       MeshPrimitives& primitives) {
    primitives.Clear();
    const bool usesNubTexture = studs == StudStyle::NubTexture;
    for (std::size_t index = 0; index < pieceCount && !primitives.HasOverflowed(); ++index) {
        Result<void> added =
            primitives.AddPiece(*input.library, input.pieces[index], usesNubTexture);
        if (!added) {
            return added;
        }
    }
    for (std::size_t index = 0;
         studs == StudStyle::Studs && index < input.studs.size() && !primitives.HasOverflowed();
         ++index) {
        Result<void> added = primitives.AddPiece(*input.library, input.studs[index], false);
        if (!added) {
            return added;
        }
    }
    if (!primitives.HasOverflowed()) {
        Finish(primitives);
    }
    return {};
}

Result<CarMesh> BuildFitting(const MeshInput& input, MeshPrimitives& primitives) {
    CarMesh mesh{.studs = input.studs.empty() ? StudStyle::NubTexture : StudStyle::Studs,
                 .drawnPieces = input.pieces.size()};
    for (;;) {
        Result<void> built = BuildPass(input, mesh.studs, mesh.drawnPieces, primitives);
        if (!built) {
            return std::unexpected(std::move(built.error()));
        }
        if (!primitives.HasOverflowed()) {
            return mesh;
        }
        if (mesh.studs != StudStyle::Plain) {
            mesh.studs = mesh.studs == StudStyle::Studs ? StudStyle::NubTexture : StudStyle::Plain;
        } else if (mesh.drawnPieces > 1) {
            --mesh.drawnPieces;
        } else {
            return Fail(ErrorCode::InvalidData, "the chassis alone overflows the car mesh");
        }
    }
}

Result<Vector3> ModelOffset(const MeshInput& input) {
    if (input.pieces.empty()) {
        return kBareOffset;
    }
    const PieceInstance& first = input.pieces.front();
    if (first.piece->modelOffsetPosition) {
        const Vector3 position = input.library->positions[*first.piece->modelOffsetPosition];
        return Vector3{.x = -position.x, .y = -position.y, .z = position.z * -kPlateHeight};
    }
    MeshPrimitives alone;
    Result<void> added = alone.AddPiece(
        *input.library, {.piece = first.piece, .placement = {.rotation = first.placement.rotation}},
        false);
    if (!added) {
        return std::unexpected(std::move(added.error()));
    }
    Vector3 low{};
    Vector3 high{};
    for (std::uint32_t index = 0; index < alone.VertexCount(); ++index) {
        const Vector3& point = alone.Vertex(index).position;
        const bool isFirst = index == 0;
        low = {.x = isFirst ? point.x : std::min(low.x, point.x),
               .y = isFirst ? point.y : std::min(low.y, point.y),
               .z = isFirst ? point.z : std::min(low.z, point.z)};
        high = {.x = isFirst ? point.x : std::max(high.x, point.x),
                .y = isFirst ? point.y : std::max(high.y, point.y),
                .z = isFirst ? point.z : std::max(high.z, point.z)};
    }
    return Vector3{
        .x = -((high.x + low.x) * 0.5F) - static_cast<float>(first.placement.x),
        .y = -((high.y + low.y) * 0.5F) - static_cast<float>(first.placement.y),
        .z = -(low.z * kPlateHeight) - (static_cast<float>(first.height) * kPlateHeight)};
}

Result<assets::GeometryAsset> ToGeometry(const MeshPrimitives& primitives, const Vector3& offset,
                                         std::span<const std::string> materials) {
    assets::GeometryAsset geometry;
    std::vector<std::uint32_t> remap(primitives.VertexCount(), kUnused);
    for (std::size_t slot = 0; slot < primitives.Live(); ++slot) {
        const MeshPrimitive& triangle = primitives.At(slot);
        if (triangle.material >= materials.size()) {
            return Fail(ErrorCode::InvalidData,
                        std::format("material {} is past the piece library's", triangle.material));
        }
        const std::uint32_t material =
            assets::FindOrAddMaterial(geometry.materials, materials[triangle.material]);
        if (geometry.ranges.empty() || geometry.ranges.back().material != material) {
            geometry.ranges.push_back(
                {.firstIndex = static_cast<std::uint32_t>(geometry.indices.size()),
                 .material = material});
        }
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const std::uint32_t source = triangle.corners[corner];
            if (remap[source] == kUnused) {
                remap[source] = static_cast<std::uint32_t>(geometry.vertices.size());
                const MeshVertex& vertex = primitives.Vertex(source);
                geometry.vertices.push_back(
                    {.position = {.x = vertex.position.x + offset.x,
                                  .y = vertex.position.y + offset.y,
                                  .z = (vertex.position.z * kPlateHeight) + offset.z},
                     .normal = {.x = static_cast<float>(vertex.normal[0]) / kNormalUnit,
                                .y = static_cast<float>(vertex.normal[1]) / kNormalUnit,
                                .z = static_cast<float>(vertex.normal[2]) / kNormalUnit},
                     .u = vertex.u,
                     .v = vertex.v});
            }
            geometry.indices.push_back(remap[source]);
        }
        geometry.ranges.back().indexCount += 3;
    }
    return geometry;
}

}

Result<CarMesh> BuildCarMesh(const BuiltCar& car, const assets::PieceCatalog& catalog) {
    Result<MeshInput> input = GatherInput(car, catalog);
    if (!input) {
        return std::unexpected(std::move(input.error()));
    }
    MeshPrimitives primitives;
    Result<CarMesh> mesh = BuildFitting(*input, primitives);
    Result<Vector3> offset = mesh.and_then([&](const CarMesh&) { return ModelOffset(*input); });
    Result<assets::GeometryAsset> geometry = offset.and_then(
        [&](const Vector3& found) { return ToGeometry(primitives, found, catalog.materials); });
    if (!geometry) {
        return std::unexpected(std::move(geometry.error()));
    }
    for (const ModelInstance& placed : input->models) {
        AppendPlacedModel(*geometry, *placed.model, placed.instance, *offset, catalog.materials);
    }
    mesh->geometry = std::move(*geometry);
    mesh->drawnPieces += input->models.size();
    return mesh;
}

Result<Vector3> CarMeshOffset(const BuiltCar& car, const assets::PieceCatalog& catalog) {
    MeshInput input{.library = &catalog.library};
    if (!car.Pieces().empty()) {
        const CarPiece& chassis = car.Pieces().front();
        const assets::PieceRecord* record =
            catalog.library.Find(chassis.piece->type, kPieceVariant);
        if (record == nullptr) {
            return Fail(ErrorCode::NotFound,
                        std::format("can't place chassis {}", chassis.piece->name));
        }
        input.pieces.push_back(
            {.piece = record, .placement = chassis.placement, .height = chassis.height});
    }
    return ModelOffset(input);
}

Result<assets::GeometryAsset> BuildPieceMesh(const assets::PieceRecord& piece, std::uint8_t colour,
                                             unsigned rotation,
                                             const assets::PieceCatalog& catalog) {
    const assets::PieceRecord* record = catalog.library.Find(piece.type, kPieceVariant);
    Result<std::uint32_t> material =
        ColourMaterials(catalog).and_then([&](const std::vector<std::uint32_t>& materials) {
            return MaterialOfColour(materials, colour);
        });
    if (!material || record == nullptr) {
        return Fail(ErrorCode::InvalidData, std::format("can't draw piece {}", piece.name));
    }
    if (const assets::GeometryAsset* model = ModelOf(*record, catalog)) {
        assets::GeometryAsset geometry;
        AppendPlacedModel(
            geometry, *model,
            {.piece = record, .placement = {.rotation = rotation}, .colourMaterial = *material}, {},
            catalog.materials);
        return geometry;
    }
    MeshInput input{.library = &catalog.library};
    input.pieces.push_back(
        {.piece = record, .placement = {.rotation = rotation}, .colourMaterial = *material});
    const assets::PieceRecord* stud = catalog.library.Find(assets::kStudPieceType, kPieceVariant);
    const assets::CellSize footprint = record->shape.Footprint(rotation);
    for (int x = 0; stud != nullptr && x < footprint.width; ++x) {
        for (int y = 0; y < footprint.height; ++y) {
            const assets::ShapeCell cell = record->shape.GetCell(x, y, rotation);
            if (cell.Occupied()) {
                input.studs.push_back({.piece = stud,
                                       .placement = {.x = x, .y = y},
                                       .height = cell.TopHeight(),
                                       .colourMaterial = *material});
            }
        }
    }
    MeshPrimitives primitives;
    return BuildFitting(input, primitives).and_then([&](const CarMesh&) {
        return ToGeometry(primitives, {}, catalog.materials);
    });
}

}
