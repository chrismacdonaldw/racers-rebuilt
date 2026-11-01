#include "render/particle_bank.hpp"

#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rr::render {
namespace {

constexpr TextureId kSmoke{7};
constexpr TextureId kDust{8};
constexpr Transform kNearCamera{.position = {.x = 1.0F}};

assets::EmitterAsset Emitter(std::string name) {
    return {.name = std::move(name),
            .emitRate = 10.0F,
            .points = {{.z = 10.0F}},
            .particleLife = Milliseconds{1000},
            .radius = 100.0F,
            .priority = 10};
}

ParticleBank Bank(std::vector<assets::EmitterAsset> emitters, std::size_t slots = 4) {
    const std::size_t count = emitters.size();
    return ParticleBank({.emitters = std::move(emitters),
                         .emitterLooks = std::vector<std::optional<ParticleLook>>(
                             count, ParticleLook{.texture = kSmoke}),
                         .slotCount = slots});
}

std::vector<QuadDraw> Quads(const ParticleBank& bank, const Transform& camera = kNearCamera) {
    std::vector<QuadDraw> quads;
    bank.AppendQuads(camera, quads);
    return quads;
}

void Run(ParticleBank& bank, float stepMs, float totalMs) {
    const auto steps = static_cast<int>(std::ceil(totalMs / stepMs));
    for (int step = 0; step < steps; ++step) {
        bank.Update(FloatMilliseconds{stepMs});
    }
}

Vector3 Centre(const QuadDraw& quad) {
    return (quad.corners[0] + quad.corners[3]) * 0.5F;
}

TEST_CASE("a particle spawns on the first update and falls under the emitter's acceleration") {
    assets::EmitterAsset falling = Emitter("dust");
    falling.acceleration = {.z = -8.0F};
    ParticleBank bank = Bank({falling});
    REQUIRE(bank.Start("dust", {}).has_value());

    bank.Update(FloatMilliseconds{0.0F});
    REQUIRE(Quads(bank).size() == 1);
    bank.Update(FloatMilliseconds{500.0F});

    const std::vector<QuadDraw> quads = Quads(bank);
    REQUIRE(quads.size() == 6);
    CHECK(Centre(quads[0]).z == doctest::Approx(4.0F));
    CHECK(quads[0].texture == kSmoke);
}

TEST_CASE("the spawn rate holds at any frame rate") {
    for (const float stepMs : {4.0F, 7.0F, 25.0F}) {
        CAPTURE(stepMs);
        ParticleBank bank = Bank({Emitter("dust")});
        const std::optional<EmitterHandle> handle = bank.Start("dust", {});
        REQUIRE(handle.has_value());

        Run(bank, stepMs, 1000.0F);

        CHECK(bank.SpawnCount(*handle) == 11);
    }
}

TEST_CASE("emit chance skips spawns, and a chance of 2 wraps to never skipping") {
    assets::EmitterAsset rare = Emitter("rare");
    rare.emitChance = 0.5F;
    assets::EmitterAsset wrapped = Emitter("wrapped");
    wrapped.emitChance = 2.0F;
    ParticleBank bank = Bank({rare, wrapped});
    const std::optional<EmitterHandle> rareHandle = bank.Start("rare", {});
    const std::optional<EmitterHandle> wrappedHandle = bank.Start("wrapped", {});
    REQUIRE((rareHandle && wrappedHandle));

    Run(bank, 100.0F, 10'000.0F);

    CHECK(bank.SpawnCount(*rareHandle) > 20);
    CHECK(bank.SpawnCount(*rareHandle) < 80);
    CHECK(bank.SpawnCount(*wrappedHandle) == 101);
}

TEST_CASE("a chance too large for a whole number never skips") {
    for (const float chance : {std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity(), -1e20F, 1e17F}) {
        CAPTURE(chance);
        assets::EmitterAsset odd = Emitter("odd");
        odd.emitChance = chance;
        ParticleBank bank = Bank({odd});
        const std::optional<EmitterHandle> handle = bank.Start("odd", {});
        REQUIRE(handle.has_value());

        Run(bank, 100.0F, 1000.0F);

        CHECK(bank.SpawnCount(*handle) == 11);
    }
}

TEST_CASE("one update spawns at most a slot's worth of particles and drops the backlog") {
    ParticleBank bank = Bank({Emitter("dust")});
    const std::optional<EmitterHandle> handle = bank.Start("dust", {});
    REQUIRE(handle.has_value());

    bank.Update(FloatMilliseconds{60'000.0F});
    CHECK(bank.SpawnCount(*handle) == kParticlesPerSlot);

    bank.Update(FloatMilliseconds{1.0F});
    CHECK(bank.SpawnCount(*handle) == kParticlesPerSlot + 1);
}

TEST_CASE("an emitter without a rate spawns once per update") {
    assets::EmitterAsset steady = Emitter("steady");
    steady.emitRate = 0.0F;
    ParticleBank bank = Bank({steady});
    const std::optional<EmitterHandle> handle = bank.Start("steady", {});
    REQUIRE(handle.has_value());

    Run(bank, 10.0F, 50.0F);

    CHECK(bank.SpawnCount(*handle) == 5);
}

TEST_CASE("a timed emitter has no handle and its particles die after it ends") {
    assets::EmitterAsset burst = Emitter("burst");
    burst.duration = Milliseconds{250};
    burst.particleLife = Milliseconds{100};
    ParticleBank bank = Bank({burst});

    CHECK_FALSE(bank.Start("burst", {}).has_value());
    Run(bank, 50.0F, 200.0F);
    CHECK_FALSE(Quads(bank).empty());
    Run(bank, 50.0F, 300.0F);
    CHECK(Quads(bank).empty());
}

TEST_CASE("a slot holds at most 16 particles, replacing the oldest") {
    assets::EmitterAsset dense = Emitter("dense");
    dense.emitRate = 1000.0F;
    dense.particleLife = Milliseconds{10'000};
    ParticleBank bank = Bank({dense});
    REQUIRE(bank.Start("dense", {}).has_value());

    Run(bank, 1.0F, 100.0F);

    CHECK(Quads(bank).size() == kParticlesPerSlot);
}

assets::EmitterAsset WithPriority(std::string name, std::uint8_t priority) {
    assets::EmitterAsset emitter = Emitter(std::move(name));
    emitter.priority = priority;
    return emitter;
}

TEST_CASE("a stronger emitter takes the weakest slot when all are busy") {
    ParticleBank bank =
        Bank({Emitter("weak"), WithPriority("strong", 20), WithPriority("middle", 15)}, 1);

    const std::optional<EmitterHandle> first = bank.Start("weak", {});
    const std::optional<EmitterHandle> second = bank.Start("strong", {});
    REQUIRE((first && second));
    CHECK_FALSE(bank.Start("middle", {}).has_value());
    bank.Update(FloatMilliseconds{0.0F});
    CHECK(bank.SpawnCount(*first) == 0);
    CHECK(bank.SpawnCount(*second) == 1);
}

TEST_CASE("a finishing emitter gives up its slot to any other") {
    ParticleBank bank = Bank({WithPriority("strong", 20), WithPriority("faint", 1)}, 1);
    const std::optional<EmitterHandle> strong = bank.Start("strong", {});
    REQUIRE(strong.has_value());
    CHECK_FALSE(bank.Start("faint", {}).has_value());

    bank.Finish(*strong);

    CHECK(bank.Start("faint", {}).has_value());
}

TEST_CASE("finishing lets particles live out while releasing removes them") {
    ParticleBank bank = Bank({Emitter("dust")});
    const std::optional<EmitterHandle> finished = bank.Start("dust", {});
    REQUIRE(finished.has_value());
    bank.Update(FloatMilliseconds{0.0F});

    bank.Finish(*finished);
    Run(bank, 100.0F, 500.0F);
    CHECK(Quads(bank).size() == 1);

    const std::optional<EmitterHandle> released = bank.Start("dust", {});
    REQUIRE(released.has_value());
    bank.Update(FloatMilliseconds{0.0F});
    bank.Release(*released);
    CHECK(Quads(bank).size() == 1);
}

TEST_CASE("spawn velocities turn with the placement and add its velocity") {
    ParticleBank bank = Bank({Emitter("dust")});
    const EmitterPlacement placement{
        .position = {.x = 5.0F},
        .rotation = FromAxisAngle({.y = 1.0F}, std::numbers::pi_v<float> / 2.0F),
        .velocity = {.y = 4.0F}};
    REQUIRE(bank.Start("dust", placement).has_value());

    bank.Update(FloatMilliseconds{0.0F});
    bank.Update(FloatMilliseconds{100.0F});

    const Vector3 centre = Centre(Quads(bank)[0]);
    CHECK(centre.x == doctest::Approx(6.0F));
    CHECK(centre.y == doctest::Approx(0.4F));
    CHECK(centre.z == doctest::Approx(0.0F).epsilon(0.001));
}

TEST_CASE("track emitters change look with particle age") {
    assets::EmitterAsset tracked = Emitter("smoke");
    tracked.materialTrack = 0;
    ParticleBank bank(
        {.emitters = {tracked},
         .emitterLooks = {std::nullopt},
         .animation = {.keys = {{.material = "smoke1"}, {.material = "smoke2", .frame = 15}},
                       .tracks = {{.keyCount = 2, .duration = 30}}},
         .keyLooks = {ParticleLook{.texture = kSmoke}, ParticleLook{.texture = kDust}},
         .slotCount = 1});
    REQUIRE(bank.Start("smoke", {}).has_value());

    bank.Update(FloatMilliseconds{0.0F});
    CHECK(Quads(bank).front().texture == kSmoke);
    bank.Update(FloatMilliseconds{600.0F});
    CHECK(Quads(bank).front().texture == kDust);
}

TEST_CASE("particles stay hidden without a look, without a size or out of range") {
    assets::EmitterAsset shrunk = Emitter("shrunk");
    shrunk.startSizeUp = 0.0F;
    ParticleBank hidden({.emitters = {Emitter("dust"), shrunk},
                         .emitterLooks = {std::nullopt, ParticleLook{}},
                         .slotCount = 2});
    REQUIRE(hidden.Start("dust", {}).has_value());
    REQUIRE(hidden.Start("shrunk", {}).has_value());
    hidden.Update(FloatMilliseconds{0.0F});
    CHECK(Quads(hidden).empty());

    ParticleBank far = Bank({Emitter("dust")});
    REQUIRE(far.Start("dust", {}).has_value());
    far.Update(FloatMilliseconds{0.0F});
    CHECK(Quads(far, {.position = {.x = 101.0F}}).empty());
    CHECK(Quads(far, {.position = {.x = 99.0F}}).size() == 1);
}

TEST_CASE("quads face the camera with full edges of the particle's size") {
    assets::EmitterAsset sized = Emitter("sized");
    sized.startSizeUp = 2.0F;
    sized.startSizeForward = 4.0F;
    ParticleBank bank = Bank({sized});
    REQUIRE(bank.Start("sized", {}).has_value());
    bank.Update(FloatMilliseconds{0.0F});

    const QuadDraw quad = Quads(bank).front();

    CHECK(quad.corners[0] == Vector3{.x = -2.0F, .y = 1.0F});
    CHECK(quad.corners[3] == Vector3{.x = 2.0F, .y = -1.0F});
    CHECK(quad.uv.x == doctest::Approx(0.01F));
}

TEST_CASE("a full race budget updates and draws in well under half a millisecond") {
    constexpr int kFrames = 1000;
    constexpr std::size_t kCommonSlots = 10;
    constexpr std::size_t kTrackSlots = 4;
    assets::EmitterAsset dense = Emitter("dense");
    dense.emitRate = 1000.0F;
    std::array banks{Bank({dense}, kCommonSlots), Bank({dense}, kTrackSlots)};
    for (ParticleBank& bank : banks) {
        while (bank.Start("dense", {}).has_value()) {
        }
    }
    std::vector<QuadDraw> quads;
    quads.reserve((kCommonSlots + kTrackSlots) * kParticlesPerSlot);
    for (ParticleBank& bank : banks) {
        Run(bank, 4.0F, 100.0F);
    }

    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < kFrames; ++frame) {
        quads.clear();
        for (ParticleBank& bank : banks) {
            bank.Update(FloatMilliseconds{4.0F});
            bank.AppendQuads(kNearCamera, quads);
        }
    }
    const std::chrono::duration<double, std::milli> elapsed =
        std::chrono::steady_clock::now() - start;

    CHECK(quads.size() == (kCommonSlots + kTrackSlots) * kParticlesPerSlot);
    MESSAGE(std::format("{:.4f} ms per frame", elapsed.count() / kFrames));
    CHECK(elapsed.count() / kFrames < 0.5);
}
}
}
