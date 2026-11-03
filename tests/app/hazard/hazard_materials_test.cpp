#include "app/hazard/hazard_materials.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <string_view>

namespace rr::app {
namespace {

constexpr render::TextureId kRed{1};
constexpr render::TextureId kPurple{2};
constexpr render::TextureId kBlue{3};

PlayedTrack Cycle() {
    return {.frames = {.keys = {{.material = "red", .frame = 0},
                                {.material = "purple", .frame = 2},
                                {.material = "blue", .frame = 5}},
                       .tracks = {{.firstKey = 0, .keyCount = 3, .duration = 6}}},
            .materials = {{.texture = kRed}, {.texture = kPurple}, {.texture = kBlue}}};
}

std::optional<std::size_t> LampOf(std::string_view entity, std::size_t track) {
    return FindHazardDriven(entity, track).transform([](const HazardDriven& driven) {
        return driven.lamp;
    });
}

std::optional<std::size_t> BlowUpOf(std::string_view entity, std::size_t track) {
    return FindHazardDriven(entity, track).transform([](const HazardDriven& driven) {
        return driven.blowUpTrack;
    });
}

render::TextureId Shown(const HazardDriven& driven, const sim::HazardMaterialsSnapshot& hazards) {
    const PlayedTrack bound = Cycle();
    const render::Material* shown = HazardShown(driven, bound, hazards);
    return shown == nullptr ? render::kWhiteTexture : shown->texture;
}

}

TEST_CASE("the code lamps and the sphinx's looping tracks are hazard driven") {
    CHECK(LampOf("mmcode1", 5) == 0U);
    CHECK(LampOf("mmcode3", 3) == 2U);
    CHECK(BlowUpOf("blowup", 1) == 3U);
    CHECK_FALSE(FindHazardDriven("mmcode4", 0));
    CHECK_FALSE(FindHazardDriven("mmcode", 0));
    CHECK_FALSE(FindHazardDriven("blowup", 2));
    CHECK_FALSE(FindHazardDriven("ktflgw01", 0));
}

TEST_CASE("a lamp holds its first frame for a red gate and its last for a blue one") {
    const HazardDriven lamp{.kind = HazardDriven::Kind::Lamp, .lamp = 1};
    sim::HazardMaterialsSnapshot hazards;
    hazards.areLampsRed = {false, true, false};

    CHECK(Shown(lamp, hazards) == kRed);
    hazards.areLampsRed[1] = false;
    CHECK(Shown(lamp, hazards) == kBlue);
}

TEST_CASE("a flashing lamp plays on from the frame it held at its own rate") {
    const HazardDriven lamp{.kind = HazardDriven::Kind::Lamp, .lamp = 1};
    sim::HazardMaterialsSnapshot hazards{.lampsFlashing = Milliseconds{250}};

    CHECK(Shown(lamp, hazards) == kRed);
    hazards.lampsFlashing = Milliseconds{750};
    CHECK(Shown(lamp, hazards) == kPurple);
}

TEST_CASE("the sphinx plays its blow-up once from the hit, then holds the last frame") {
    HazardDriven sphinx{.kind = HazardDriven::Kind::Sphinx};
    sphinx.blowUp = Cycle();
    sim::HazardMaterialsSnapshot hazards;

    CHECK(HazardShown(sphinx, Cycle(), hazards) == nullptr);
    hazards.sphinxBlowingUp = Milliseconds{70};
    CHECK(Shown(sphinx, hazards) == kPurple);
    hazards.sphinxBlowingUp = Milliseconds{400};
    CHECK(Shown(sphinx, hazards) == kBlue);
}

}
