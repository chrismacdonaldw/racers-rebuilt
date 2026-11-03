#include "app/track_material_anims.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "app/track_entity_draws.hpp"

namespace rr::app {
namespace {

constexpr render::TextureId kOn{1};
constexpr render::TextureId kOff{2};

class Fixture {
public:
    Fixture() {
        world_.materialAnimations = {"lights"};
        animations_.emplace(
            "LIGHTS",
            std::make_shared<const assets::MaterialAnimationAsset>(assets::MaterialAnimationAsset{
                .keys = {{.material = "on", .frame = 0},
                         {.material = "off", .frame = 3},
                         {.material = "off", .frame = 0}},
                .tracks = {{.firstKey = 0, .keyCount = 2, .duration = 6},
                           {.firstKey = 2, .keyCount = 1, .duration = 1}}}));
        materials_.emplace("ON", render::Material{.texture = kOn});
        materials_.emplace("OFF", render::Material{.texture = kOff});
    }

    [[nodiscard]] MaterialSources Sources() const {
        return {.animations = &animations_, .materials = &materials_};
    }

    [[nodiscard]] const assets::WorldAsset& World() const { return world_; }

private:
    assets::WorldAsset world_;
    assets::NameMap<assets::AssetCache<assets::MaterialAnimationAsset>::Handle> animations_;
    assets::NameMap<render::Material> materials_;
};

assets::WorldEntity Bound(std::int32_t track, std::int32_t slot) {
    return {.name = "lamp",
            .materialAnimations = {{.animation = 0, .track = track, .material = slot}}};
}

std::vector<render::TextureId> Textures(std::span<const render::MeshDraw> draws) {
    std::vector<render::TextureId> textures;
    for (const render::MeshDraw& draw : draws) {
        textures.push_back(draw.material.texture);
    }
    return textures;
}

TEST_CASE("a bound track swaps the material of its slot's draws over its cycle") {
    const Fixture fixture;
    TrackMaterialAnims anims;
    const std::array<std::uint32_t, 3> slots{1, 0, 1};
    anims.Bind(fixture.World(), Bound(0, 1), slots, 2, fixture.Sources());
    std::vector<render::MeshDraw> draws(5);
    draws[2].material.uvOffset = {.u = 0.25F};

    anims.Apply(50.0F, draws.size(), draws);
    const std::vector<render::TextureId> early = Textures(draws);
    anims.Apply(150.0F, draws.size(), draws);

    CHECK(early == std::vector<render::TextureId>{render::kWhiteTexture, render::kWhiteTexture, kOn,
                                                  render::kWhiteTexture, kOn});
    CHECK(Textures(draws)[2] == kOff);
    CHECK(Textures(draws)[4] == kOff);
    CHECK(draws[2].material.uvOffset.u == 0.25F);
}

TEST_CASE("the last binding of a track takes it over") {
    const Fixture fixture;
    TrackMaterialAnims anims;
    const std::array<std::uint32_t, 1> slot{0};
    anims.Bind(fixture.World(), Bound(1, 0), slot, 0, fixture.Sources());
    anims.Bind(fixture.World(), Bound(1, 0), slot, 1, fixture.Sources());
    std::vector<render::MeshDraw> draws(2);

    anims.Apply(0.0F, draws.size(), draws);

    CHECK(Textures(draws) == std::vector<render::TextureId>{render::kWhiteTexture, kOff});
}

TEST_CASE("a binding past the animation's tracks is left out") {
    const Fixture fixture;
    TrackMaterialAnims anims;
    const std::array<std::uint32_t, 1> slot{0};
    anims.Bind(fixture.World(), Bound(5, 0), slot, 0, fixture.Sources());
    std::vector<render::MeshDraw> draws(1);

    anims.Apply(0.0F, draws.size(), draws);

    CHECK(Textures(draws)[0] == render::kWhiteTexture);
}

TEST_CASE("an animated entity's binding finds its draws by name") {
    const Fixture fixture;
    TrackMaterialAnims anims;
    anims.Bind(fixture.World(), Bound(0, 2), {}, 0, fixture.Sources());
    const sim::TrackEntitySources sources{.skeletons = {{.nodes = {{.name = "root"}}}},
                                          .animations = {{}},
                                          .entities = {{.name = "lamp", .model = "lamp"}}};
    const assets::NameMap<JointedModel> models{
        {"LAMP", {.draws = std::vector<render::MeshDraw>(2), .joints = {0, 0}, .slots = {2, 0}}}};
    const TrackEntityDraws entityDraws(models, sources);
    anims.FindEntities(sources, entityDraws);
    std::vector<render::MeshDraw> draws(4);

    anims.Apply(0.0F, 2, draws);

    CHECK(Textures(draws) == std::vector<render::TextureId>{render::kWhiteTexture,
                                                            render::kWhiteTexture, kOn,
                                                            render::kWhiteTexture});
}

}
}
