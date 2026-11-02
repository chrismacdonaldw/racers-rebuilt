#include "app/cutscene/cutscene_particles.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/track_space.hpp"
#include "assets/archive_survey.hpp"

namespace rr::app {
namespace {

constexpr Transform kCamera{.position = {.x = 1.0F}};
constexpr assets::CutsceneAxes kAxes{.direction = {.x = 1.0F}, .up = {.z = 1.0F}};

std::vector<render::ParticleBank> Bubbles() {
    std::vector<render::ParticleBank> banks;
    banks.emplace_back(render::ParticleBankSetup{.emitters = {{.name = "bubbles",
                                                               .emitRate = 10.0F,
                                                               .points = {{.z = 10.0F}},
                                                               .particleLife = Milliseconds{1000},
                                                               .radius = 100.0F}},
                                                 .emitterLooks = {render::ParticleLook{}},
                                                 .slotCount = 3});
    return banks;
}

std::vector<render::QuadDraw> Quads(const CutsceneParticles& particles) {
    std::vector<render::QuadDraw> quads;
    particles.AppendQuads(kCamera, quads);
    return quads;
}

Vector3 Centre(const render::QuadDraw& quad) {
    return (quad.corners[0] + quad.corners[3]) * 0.5F;
}

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

TEST_CASE("a particle effect spawns at its transform once, until a stop clears it at once") {
    const std::vector<assets::CutsceneParticle> effects{{.name = "bub1", .emitter = "bubbles"}};
    CutsceneParticles particles(effects, Bubbles());
    const Vector3 transform{.x = 5.0F, .y = 2.0F};

    particles.Start(0, transform, kAxes);
    particles.Update(Milliseconds{0});
    particles.Start(0, transform, kAxes);
    particles.Update(Milliseconds{0});
    const std::vector<render::QuadDraw> shown = Quads(particles);
    REQUIRE(shown.size() == 1);
    CheckNear(Centre(shown[0]), ToEngine(transform));

    particles.Stop(0);
    CHECK(Quads(particles).empty());

    particles.Start(0, transform, kAxes);
    particles.Update(Milliseconds{0});
    CHECK(Quads(particles).size() == 1);
}

TEST_CASE("a particle's own position wins over its transform's") {
    const std::vector<assets::CutsceneParticle> effects{
        {.name = "bub1", .emitter = "bubbles", .position = Vector3{.z = 3.0F}}};
    CutsceneParticles particles(effects, Bubbles());

    particles.Start(0, {.x = 5.0F}, kAxes);
    particles.Update(Milliseconds{0});

    const std::vector<render::QuadDraw> shown = Quads(particles);
    REQUIRE(shown.size() == 1);
    CheckNear(Centre(shown[0]), ToEngine({.z = 3.0F}));
}

Result<std::vector<render::ParticleBank>> LoadRetail(const assets::Vfs& vfs,
                                                     std::string_view folder, std::string library,
                                                     std::string emitter) {
    const assets::CutsceneEffectsAsset effects{
        .particleLibraries = {std::move(library)},
        .particles = {{.name = "bub1", .emitter = std::move(emitter)}}};
    return LoadCutsceneParticles(vfs, folder, effects, {}, {});
}

TEST_CASE("retail cutscene particle libraries load") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<std::vector<render::ParticleBank>> bubbles =
        LoadRetail(*vfs, "MENUDATA/CIRCUIT3", "cir3-01", "bubbles");
    REQUIRE_MESSAGE(bubbles.has_value(), bubbles.error().message);
    CHECK(bubbles->size() == 1);

    const Result<std::vector<render::ParticleBank>> snowing =
        LoadRetail(*vfs, "MENUDATA/SINGRACE", "pst", "snow");
    REQUIRE_MESSAGE(snowing.has_value(), snowing.error().message);
}

TEST_CASE("a particle whose emitter isn't in its library fails") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<std::vector<render::ParticleBank>> missing =
        LoadRetail(*vfs, "MENUDATA/CIRCUIT3", "cir3-01", "snow");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().message == "particle bub1 runs snow, which cir3-01 doesn't have");
}

}
}
