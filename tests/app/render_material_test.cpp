#include <doctest/doctest.h>

#include "app/track_model.hpp"
#include "assets/material_library.hpp"
#include "render/render_view.hpp"

namespace rr::app {
namespace {

render::Material Converted(const assets::MaterialAsset& material) {
    return ToRenderMaterial(material, {});
}

}

TEST_CASE("a material samples the way its MDB flags ask") {
    CHECK(Converted({}).filter == render::TextureFilter::Linear);
    CHECK(Converted({}).address == render::TextureAddress::Repeat);
    CHECK(Converted({}).shading == render::Shading::Gouraud);

    CHECK(Converted({.filter = assets::TextureFilter::Point}).filter ==
          render::TextureFilter::Point);
    CHECK(Converted({.address = assets::TextureAddress::Clamp}).address ==
          render::TextureAddress::Clamp);
    CHECK(Converted({.shading = assets::Shading::Flat}).shading == render::Shading::Flat);
}

}
