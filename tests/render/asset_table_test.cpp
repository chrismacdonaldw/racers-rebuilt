#include "render/asset_table.hpp"

#include <doctest/doctest.h>

#include <string>

#include "render/render_view.hpp"

namespace rr::render {
namespace {

TEST_CASE("released assets are gone and pinned ones stay") {
    AssetTable<std::string, TextureId> textures;
    REQUIRE(textures.Add("white") == kWhiteTexture);
    textures.Pin();
    const TextureId track = textures.Add("track");

    textures.Release();

    CHECK(textures.Get(kWhiteTexture) == "white");
    CHECK_FALSE(textures.Contains(track));
    CHECK(textures.Add("next track") == track);
}

TEST_CASE("without a pin, release drops everything") {
    AssetTable<std::string, MeshId> meshes;
    const MeshId cube = meshes.Add("cube");

    meshes.Release();

    CHECK_FALSE(meshes.Contains(cube));
}

}
}
