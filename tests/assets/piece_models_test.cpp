#include "assets/piece_models.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "assets/folder_layer.hpp"
#include "assets/geometry_writer.hpp"
#include "temp_folder.hpp"

namespace rr::assets {
namespace {

void WriteBytes(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::byte> Triangle() {
    const GeometryAsset geometry{.vertices = {{.position = {}, .normal = {.z = 1.0F}},
                                              {.position = {.x = 1.0F}, .normal = {.z = 1.0F}},
                                              {.position = {.y = 1.0F}, .normal = {.z = 1.0F}}},
                                 .indices = {0, 1, 2},
                                 .materials = {"piece"},
                                 .ranges = {{.firstIndex = 0, .indexCount = 3, .material = 0}}};
    Result<std::vector<std::byte>> bytes = WriteGeometry(geometry, GdbVertices::Lit);
    REQUIRE(bytes.has_value());
    return std::move(*bytes);
}

PieceRecord Named(std::string name, std::uint16_t type) {
    return {.name = std::move(name), .type = type};
}

TEST_CASE("a piece's model path names it back") {
    CHECK(PieceModelPath("l300100") == "MENUDATA/PIECEDB/MODELS/L300100.GDB");
    CHECK(PieceOfModelPath("MENUDATA/PIECEDB/MODELS/L300100.GDB") == "L300100");
    CHECK_FALSE(PieceOfModelPath("MENUDATA/PIECEDB/L300100.GDB").has_value());
    CHECK_FALSE(PieceOfModelPath("MENUDATA/PIECEDB/MODELS/SUB/L300100.GDB").has_value());
    CHECK_FALSE(PieceOfModelPath("MENUDATA/PIECEDB/MODELS/L300100.ADB").has_value());
    CHECK_FALSE(PieceOfModelPath("MENUDATA/PIECEDB/MODELS/.GDB").has_value());
}

TEST_CASE("bricks with a readable model file get it, chassis and broken files don't") {
    const test::TempFolder folder;
    const std::filesystem::path models = folder.Path() / "menudata/piecedb/models";
    const std::vector<std::byte> triangle = Triangle();
    WriteBytes(models / "brick.gdb", triangle);
    WriteBytes(models / "chassis.gdb", triangle);
    WriteBytes(models / "broken.gdb", std::vector<std::byte>(4, std::byte{0x7F}));
    Result<FolderLayer> layer = FolderLayer::Scan(folder.Path());
    REQUIRE(layer.has_value());
    Vfs vfs;
    vfs.Mount(std::make_unique<FolderLayer>(std::move(*layer)));
    const PieceLibrary library{.pieces = {Named("chassis", 17), Named("Brick", 0x900),
                                          Named("broken", 0x901), Named("missing", 0x902)}};

    const PieceModels read = ReadPieceModels(vfs, library);

    REQUIRE(read.size() == 1);
    REQUIRE(read.contains("BRICK"));
    CHECK(read.at("BRICK").indices.size() == 3);
    CHECK(read.at("BRICK").materials == std::vector<std::string>{"piece"});
}

}
}
