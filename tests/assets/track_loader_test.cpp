#include "assets/track_loader.hpp"

#include <doctest/doctest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "assets/folder_layer.hpp"
#include "assets/game_path.hpp"
#include "assets/token_bytes.hpp"
#include "temp_folder.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Join;
using test::ListHeader;
using test::Text;

std::vector<std::byte> NameList(std::uint8_t field, std::string_view name) {
    return Join({Bytes({field}), ListHeader(1), Text(name), Bytes({0x06})});
}

std::vector<std::byte> OneEntryLibrary(std::string_view name) {
    return Join(
        {Bytes({0x27}), ListHeader(1), Bytes({0x27}), Text(name), Bytes({0x05, 0x06, 0x06})});
}

std::vector<std::byte> OnePixelTga() {
    return Bytes({0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                  0x00, 0x01, 0x00, 0x01, 0x00, 0x20, 0x08, 0x01, 0x02, 0x03, 0x04});
}

class TrackFixture {
public:
    TrackFixture() {
        Write("gamedata/racex/world.wdb", Join({NameList(0x27, "tex"), NameList(0x28, "mat")}));
        Write("gamedata/racex/mat.mdb", OneEntryLibrary("Glass"));
        Write("gamedata/racex/tex.tdb", OneEntryLibrary("wood"));
        Write("gamedata/common/wood.tga", OnePixelTga());
    }

    void Write(std::string_view path, std::span<const std::byte> bytes) const {
        const std::filesystem::path file = folder_.Path() / path;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary)
            .write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }

    void Remove(std::string_view path) const { std::filesystem::remove(folder_.Path() / path); }

    Result<void> Mount() {
        return FolderLayer::Scan(folder_.Path()).transform([&](FolderLayer layer) {
            vfs_.Mount(std::make_unique<FolderLayer>(std::move(layer)));
        });
    }

    Result<TrackAssets> Load() {
        return Mount().and_then([&] { return loader_.Load("gamedata/racex"); });
    }

protected:
    test::TempFolder folder_;
    Vfs vfs_;
    LoaderPool pool_{2};
    TrackLoader loader_{vfs_, pool_};
};

TEST_CASE_FIXTURE(TrackFixture, "a track loads its worlds and what they name") {
    const Result<TrackAssets> track = Load();

    REQUIRE(track.has_value());
    CHECK(track->worlds.size() == 1);
    CHECK(track->materials.contains("GLASS"));
    REQUIRE(track->textures.contains("WOOD"));
    CHECK(track->textures.at("WOOD")->widthPx == 1);
}

TEST_CASE_FIXTURE(TrackFixture, "loading a track again reuses the cached files") {
    const Result<TrackAssets> first = Load();
    const Result<TrackAssets> second = loader_.Load("GAMEDATA/RACEX");

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(first->worlds.front() == second->worlds.front());
    CHECK(first->textures.at("WOOD") == second->textures.at("WOOD"));
}

TEST_CASE_FIXTURE(TrackFixture, "models load the files they name without worlds") {
    Write("gamedata/common/car.mdb", OneEntryLibrary("Paint"));
    const std::array models{ModelFiles{.materials = "car", .textures = "", .geometry = ""},
                            ModelFiles{.materials = "mat", .textures = "tex", .geometry = ""}};

    const Result<TrackAssets> loaded =
        Mount().and_then([&] { return loader_.LoadModels("gamedata/racex", models); });

    REQUIRE(loaded.has_value());
    CHECK(loaded->worlds.empty());
    CHECK(loaded->materials.contains("PAINT"));
    CHECK(loaded->materials.contains("GLASS"));
    CHECK(loaded->textures.contains("WOOD"));
}

TEST_CASE_FIXTURE(TrackFixture, "a missing texture file is skipped and named") {
    Remove("gamedata/common/wood.tga");

    const Result<TrackAssets> track = Load();

    REQUIRE(track.has_value());
    CHECK(track->textures.empty());
    REQUIRE(track->missingTextures.size() == 1);
    CHECK(assets::ToUpperAscii(track->missingTextures.front()) == "WOOD.TGA");
}

}
}
