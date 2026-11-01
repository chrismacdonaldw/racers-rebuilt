#include "app/powerup/powerup_quads.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "app/track_space.hpp"
#include "sim/powerup/dropped_trap.hpp"
#include "sim/powerup/grapple_effect.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::app {
namespace {

constexpr float kFrameMs = 16.0F;
constexpr Vector3 kOwner{.x = 10.0F, .y = 20.0F};

render::TextureId TextureOf(std::string_view material) {
    const auto names = PowerupQuadMaterials();
    const auto found = std::ranges::find(names, material);
    return render::TextureId{static_cast<std::uint32_t>(100 + (found - names.begin()))};
}

assets::NameMap<render::Material> AllMaterials() {
    assets::NameMap<render::Material> materials;
    for (const std::string_view name : PowerupQuadMaterials()) {
        materials.emplace(std::string(name), render::Material{.texture = TextureOf(name)});
    }
    return materials;
}

std::size_t CountOf(const PowerupQuads& quads, std::string_view material) {
    return static_cast<std::size_t>(
        std::ranges::count(quads.Quads(), TextureOf(material), &render::QuadDraw::texture));
}

sim::RenderSnapshot WithEffect(sim::PowerupKind kind, std::uint8_t phase, const Vector3& at) {
    sim::RenderSnapshot snapshot;
    snapshot.entities.push_back(
        {.id = sim::EntityId{1}, .transform = TransformToEngine({.position = kOwner})});
    snapshot.effects.push_back({.slot = 2,
                                .kind = kind,
                                .use = {.racer = 0},
                                .phase = phase,
                                .transform = TransformToEngine({.position = at})});
    return snapshot;
}

TEST_CASE("a flying cannonball draws its billboard and a smoke trail behind it") {
    const assets::NameMap<render::Material> materials = AllMaterials();
    PowerupQuads quads(materials);
    const auto flying = static_cast<std::uint8_t>(sim::ShotPhase::Flying);

    for (int frame = 0; frame < 10; ++frame) {
        quads.Update(WithEffect(sim::PowerupKind::Cannonball, flying,
                                {.x = 5.0F * static_cast<float>(frame)}),
                     {}, kFrameMs);
    }

    CHECK(CountOf(quads, "CANNON") == 1);
    CHECK(CountOf(quads, "CANSTRK") > 0);
    quads.Update(WithEffect(sim::PowerupKind::Cannonball,
                            static_cast<std::uint8_t>(sim::ShotPhase::Done), {}),
                 {}, kFrameMs);
    CHECK(quads.Quads().empty());
}

TEST_CASE("a grappling rope runs from above its owner to the hook, sagging until it pulls") {
    const assets::NameMap<render::Material> materials = AllMaterials();
    PowerupQuads quads(materials);
    const Vector3 hook = kOwner + Vector3{.y = 40.0F, .z = 3.0F};
    const auto lowest = [&] {
        float low = 100.0F;
        for (const render::QuadDraw& quad : quads.Quads()) {
            for (const Vector3& corner : quad.corners) {
                low = std::min(low, ToGame(corner).z);
            }
        }
        return low;
    };

    quads.Update(WithEffect(sim::PowerupKind::GrapplingHook,
                            static_cast<std::uint8_t>(sim::GrapplePhase::Flying), hook),
                 {.position = {.y = 100.0F}}, kFrameMs);
    CHECK(CountOf(quads, "TETHER") == 8);
    CHECK(lowest() < 0.0F);

    quads.Update(WithEffect(sim::PowerupKind::GrapplingHook,
                            static_cast<std::uint8_t>(sim::GrapplePhase::Pulling), hook),
                 {.position = {.y = 100.0F}}, kFrameMs);
    CHECK(lowest() == doctest::Approx(3.0F).epsilon(0.01));
}

TEST_CASE("a lightning beam jitters between its source and the bolt's end") {
    const assets::NameMap<render::Material> materials = AllMaterials();
    PowerupQuads quads(materials);
    const Vector3 end = kOwner + Vector3{.x = 200.0F, .z = 3.0F};

    quads.Update(WithEffect(sim::PowerupKind::Lightning, 1, end), {.position = {.y = 100.0F}},
                 kFrameMs);

    REQUIRE(CountOf(quads, "LIGHTNG") == 4);
    const Vector3 first = ToGame(quads.Quads().front().corners[0]);
    const Vector3 last = ToGame(quads.Quads().back().corners[3]);
    CHECK(first.x == doctest::Approx(kOwner.x).epsilon(0.01));
    CHECK(last.x == doctest::Approx(end.x).epsilon(0.01));
}

TEST_CASE("an oil slick lies on the track only once armed") {
    const assets::NameMap<render::Material> materials = AllMaterials();
    PowerupQuads quads(materials);

    quads.Update(WithEffect(sim::PowerupKind::OilSlick,
                            static_cast<std::uint8_t>(sim::TrapPhase::Dropping), kOwner),
                 {}, kFrameMs);
    CHECK(quads.Quads().empty());
    quads.Update(WithEffect(sim::PowerupKind::OilSlick,
                            static_cast<std::uint8_t>(sim::TrapPhase::Armed), kOwner),
                 {}, kFrameMs);
    CHECK(CountOf(quads, "OILSLCK") == 1);
}

TEST_CASE("blasts flash as they grow, and both kinds leave a fading scar") {
    const assets::NameMap<render::Material> materials = AllMaterials();
    PowerupQuads quads(materials);
    sim::RenderSnapshot snapshot;
    snapshot.explosions = {{.kind = sim::ExplosionKind::Blast, .transform = {}, .growth = 0.5F},
                           {.kind = sim::ExplosionKind::Spike, .transform = {}, .growth = 0.5F},
                           {.kind = sim::ExplosionKind::Spike, .transform = {}, .scarLeft = 0.5F}};

    quads.Update(snapshot, {}, kFrameMs);

    REQUIRE(quads.Quads().size() == 2);
    const render::QuadDraw& flash = quads.Quads()[0];
    CHECK(flash.texture == TextureOf("EXFLASH"));
    CHECK(Length(flash.corners[1] - flash.corners[0]) == doctest::Approx(7.55F));
    const render::QuadDraw& scar = quads.Quads()[1];
    CHECK(scar.texture == TextureOf("EXSCAR"));
    CHECK(scar.tint.a == doctest::Approx(90.0F / 255.0F));
    CHECK(Length(scar.corners[1] - scar.corners[0]) == doctest::Approx(5.0F));
}

TEST_CASE("without materials nothing draws") {
    const assets::NameMap<render::Material> none;
    PowerupQuads quads(none);
    sim::RenderSnapshot snapshot =
        WithEffect(sim::PowerupKind::Lightning, 1, kOwner + Vector3{.x = 50.0F});
    snapshot.explosions = {{.kind = sim::ExplosionKind::Blast, .transform = {}, .growth = 0.5F}};

    quads.Update(snapshot, {}, kFrameMs);

    CHECK(quads.Quads().empty());
}

}
}
