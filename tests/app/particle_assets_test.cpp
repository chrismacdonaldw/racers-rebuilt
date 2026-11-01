#include "app/particle_assets.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/surface_table.hpp"

namespace rr::app {
namespace {

constexpr render::TextureId kSmoke{4};

assets::NameMap<assets::MaterialAsset> Materials() {
    assets::NameMap<assets::MaterialAsset> materials;
    materials["SMOKE1"] = {.diffuse = {255, 0, 0, 255},
                           .texture = "smoke",
                           .textureMode = assets::TextureMode::Modulate};
    materials["SPARK"] = {.diffuse = {255, 0, 0, 255},
                          .texture = "smoke",
                          .blend = assets::BlendFactors{.destination = assets::BlendFactor::One}};
    return materials;
}

ParticleSources Sources() {
    return {.emitters = {{.name = "dust",
                          .acceleration = {.z = -10.0F},
                          .points = {{.x = 1.0F, .y = 2.0F, .z = 3.0F}},
                          .material = "spark"},
                         {.name = "smoke", .materialTrack = 0, .material = "missing"}},
            .animation = {.keys = {{.material = "smoke1"}, {.material = "gone"}},
                          .tracks = {{.keyCount = 2}}}};
}

TEST_CASE("particle setups move emitters into engine space and give them material looks") {
    const assets::NameMap<render::TextureId> textures{{"SMOKE", kSmoke}};

    const render::ParticleBankSetup setup = ToParticleSetup(Sources(), Materials(), textures, 3);

    REQUIRE(setup.emitters.size() == 2);
    CHECK(setup.emitters[0].points[0] == Vector3{.x = 1.0F, .y = 3.0F, .z = -2.0F});
    CHECK(setup.emitters[0].acceleration == Vector3{.y = -10.0F});
    REQUIRE(setup.emitterLooks.size() == 2);
    const render::ParticleLook spark = setup.emitterLooks[0].value_or(render::ParticleLook{});
    CHECK(spark.texture == kSmoke);
    CHECK(spark.blend == render::BlendMode::Additive);
    CHECK(spark.tint.g == doctest::Approx(1.0F));
    CHECK_FALSE(setup.emitterLooks[1].has_value());
    REQUIRE(setup.keyLooks.size() == 2);
    const render::ParticleLook smoke =
        setup.keyLooks[0].value_or(render::ParticleLook{.blend = render::BlendMode::Opaque});
    CHECK(smoke.blend == render::BlendMode::Alpha);
    CHECK(smoke.tint.g == doctest::Approx(0.0F));
    CHECK_FALSE(setup.keyLooks[1].has_value());
    CHECK(setup.slotCount == 3);
}

bool HasEmitter(const ParticleSources& sources, const std::string& name) {
    return std::ranges::contains(sources.emitters, name, &assets::EmitterAsset::name);
}

std::vector<std::string> WheelParticles(const assets::Vfs& vfs, const std::string& track) {
    const Result<std::vector<assets::SurfaceEntry>> surfaces =
        vfs.Read(std::format("GAMEDATA/{}/{}.TMB", track, track))
            .and_then([](const assets::FileData& data) {
                return assets::ParseSurfaceTable(data.Bytes());
            });
    REQUIRE(surfaces.has_value());
    std::vector<std::string> names;
    for (const assets::SurfaceEntry& entry : *surfaces) {
        if (entry.particle) {
            names.push_back(*entry.particle);
        }
    }
    return names;
}

void CheckTrack(const assets::Vfs& vfs, const ParticleSources& common, const std::string& track) {
    CAPTURE(track);
    const Result<ParticleSources> sources =
        TrackParticleFiles(vfs, track).and_then([&](const ParticleFiles& files) {
            return ReadParticleSources(vfs, files);
        });
    REQUIRE(sources.has_value());
    for (const std::string& name : WheelParticles(vfs, track)) {
        CAPTURE(name);
        CHECK((HasEmitter(common, name) || HasEmitter(*sources, name)));
    }
}

TEST_CASE("every retail track's particles read and its wheel particles have emitters") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<ParticleSources> common = ReadParticleSources(*vfs, CommonParticleFiles());
    REQUIRE(common.has_value());

    for (const std::string& track : assets::ListRaceTracks(*vfs)) {
        CheckTrack(*vfs, *common, track);
    }
}

TEST_CASE("retail tracks without particle materials still name their files") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<ParticleFiles> grass = TrackParticleFiles(*vfs, "RACEC0R0");
    const Result<ParticleFiles> bare = TrackParticleFiles(*vfs, "RACEC2R3");

    REQUIRE((grass && bare));
    CHECK(*grass == ParticleFiles{.folder = "GAMEDATA/RACEC0R0",
                                  .emitters = "RACEC0R0",
                                  .animation = "EMITC0R0",
                                  .materials = "EMITC0R0"});
    CHECK(bare->animation == "EMITC2R3");
    CHECK_FALSE(bare->materials.has_value());
}

}
}
