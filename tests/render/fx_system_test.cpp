#include "render/fx_system.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "render/fx_textures.hpp"
#include "render/particle_bank.hpp"

namespace rr::render {
namespace {

constexpr TextureId kSmoke{3};
constexpr TextureId kSpark{4};
constexpr TextureId kGlow{5};
constexpr TextureId kRing{6};
constexpr TextureId kBubble{7};
constexpr TextureId kStar{8};
constexpr TextureId kOriginal{9};
constexpr FxAssets kTextures{.smoke = kSmoke,
                             .spark = kSpark,
                             .glow = kGlow,
                             .ring = kRing,
                             .bubble = kBubble,
                             .star = kStar};
constexpr Transform kCamera{.position = {.z = -50.0F}};
constexpr Vector3 kSwirlMiddle{.y = 2.0F};

FxEffect Puff(float rate, int burst = 0) {
    return {.emitter = "puff",
            .ratePerSecond = rate,
            .burst = burst,
            .lifeMs = {.min = 1000.0F, .max = 1000.0F},
            .startSize = {.min = 1.0F, .max = 1.0F},
            .endSize = {.min = 4.0F, .max = 4.0F}};
}

FxSystem System(FxEffect effect, std::size_t capacity = kFxCapacity) {
    return {{effect}, kTextures, capacity};
}

Vector3 Centre(const QuadDraw& quad) {
    return (quad.corners[0] + quad.corners[3]) * 0.5F;
}

std::vector<QuadDraw> Quads(const FxSystem& fx) {
    std::vector<QuadDraw> quads;
    fx.AppendQuads(kCamera, quads);
    return quads;
}

float Alpha(const assets::ImageAsset& image, std::uint32_t x, std::uint32_t y) {
    return static_cast<float>(image.rgba.at((((y * image.widthPx) + x) * 4) + 3)) / 255.0F;
}

TEST_CASE("an effect replaces the emitter it names") {
    const FxSystem fx = System(Puff(10.0F));

    CHECK(fx.EffectFor("puff").has_value());
    CHECK_FALSE(fx.EffectFor("spark").has_value());
}

TEST_CASE("emitting keeps the rate across frames and bursts once") {
    FxSystem fx = System(Puff(100.0F, 5));
    const FxEffectId puff = *fx.EffectFor("puff");
    FxEmission emission;

    for (int frame = 0; frame < 10; ++frame) {
        fx.Emit(puff, {.position = {.x = static_cast<float>(frame)}}, 10.0F, emission);
    }

    CHECK(fx.Count() == 5 + 10);
}

TEST_CASE("particles spawn along the emitter's path, not in one clump") {
    FxSystem fx = System(Puff(1000.0F));
    const FxEffectId puff = *fx.EffectFor("puff");
    FxEmission emission;
    fx.Emit(puff, {}, 0.0F, emission);

    fx.Emit(puff, {.position = {.x = 10.0F}}, 10.0F, emission);

    const std::vector<QuadDraw> quads = Quads(fx);
    REQUIRE(quads.size() == 10);
    float least = 10.0F;
    float most = 0.0F;
    for (const QuadDraw& quad : quads) {
        least = std::min(least, Centre(quad).x);
        most = std::max(most, Centre(quad).x);
    }
    CHECK(least < 2.0F);
    CHECK(most > 8.0F);
}

TEST_CASE("an emitter that jumps spawns nothing along the gap") {
    FxSystem fx = System(Puff(1000.0F));
    const FxEffectId puff = *fx.EffectFor("puff");
    FxEmission emission;
    fx.Emit(puff, {}, 0.0F, emission);

    fx.Emit(puff, {.position = {.x = kFxMaxEmitStep * 2.0F}}, 10.0F, emission);

    for (const QuadDraw& quad : Quads(fx)) {
        CHECK(Centre(quad).x == doctest::Approx(kFxMaxEmitStep * 2.0F));
    }
}

TEST_CASE("layers naming the same emitter play together and bursts fire once") {
    FxEffect glow = Puff(0.0F, 1);
    glow.shape = FxShape::Glow;
    FxSystem fx({Puff(0.0F, 2), glow, FxEffect{.emitter = "other", .burst = 5}}, kTextures);
    FxEmission emission;

    fx.Emit(*fx.EffectFor("puff"), {}, 0.0F, emission);
    fx.Emit(*fx.EffectFor("puff"), {}, 0.0F, emission);
    fx.Burst(*fx.EffectFor("puff"), {});

    CHECK(fx.Count() == 6);
    const std::vector<QuadDraw> quads = Quads(fx);
    CHECK(std::ranges::count(quads, kGlow, &QuadDraw::texture) == 2);
}

TEST_CASE("streaks stretch along their motion and bounce off the floor") {
    FxEffect spark = Puff(0.0F, 1);
    spark.shape = FxShape::Streak;
    spark.launch = {.x = 100.0F, .y = -10.0F};
    spark.streakSeconds = 0.1F;
    spark.bounce = 0.5F;
    spark.floorDrop = 1.0F;
    FxSystem fx = System(spark);
    FxEmission emission;
    fx.Emit(*fx.EffectFor("puff"), {}, 0.0F, emission);

    fx.Update(FloatMilliseconds{150.0F});

    const QuadDraw quad = Quads(fx).front();
    CHECK(quad.texture == kSpark);
    CHECK(quad.corners[0].x - quad.corners[2].x == doctest::Approx(7.0F).epsilon(0.02));
    CHECK((quad.corners[0].y + quad.corners[1].y) * 0.5F == doctest::Approx(-1.0F));
}

TEST_CASE("particles past the draw distance are left out") {
    FxSystem fx = System(Puff(0.0F, 1));
    FxEmission emission;
    fx.Emit(*fx.EffectFor("puff"), {.position = {.z = kFxDrawDistance * 2.0F}}, 0.0F, emission);

    CHECK(fx.Count() == 1);
    CHECK(Quads(fx).empty());
}

TEST_CASE("bricks tumble as models in their palette's colours and rings lie flat") {
    constexpr std::array kPaints{Color{.r = 1.0F, .g = 0.0F, .b = 0.0F}};
    FxEffect brick = Puff(0.0F, 3);
    brick.shape = FxShape::Brick;
    brick.palette = kPaints;
    FxEffect ring = Puff(0.0F, 1);
    ring.shape = FxShape::Ring;
    ring.offset = {.y = -2.0F};
    FxSystem fx({brick, ring}, kTextures);
    FxEmission emission;
    fx.Emit(*fx.EffectFor("puff"), {.position = {.y = 5.0F}}, 0.0F, emission);

    std::vector<MeshDraw> meshes;
    fx.AppendMeshes(meshes);
    const std::vector<QuadDraw> quads = Quads(fx);

    REQUIRE(meshes.size() == 3);
    CHECK(meshes.front().material.tint.g == doctest::Approx(0.0F));
    REQUIRE(quads.size() == 1);
    CHECK(quads.front().texture == kRing);
    CHECK(std::ranges::all_of(quads.front().corners,
                              [](const Vector3& corner) { return corner.y == 3.0F; }));
}

TEST_CASE("the brick model is closed and faces outward") {
    const assets::GeometryAsset brick = MakeBrickGeometry();

    REQUIRE_FALSE(brick.indices.empty());
    CHECK(brick.indices.size() % 3 == 0);
    CHECK(std::ranges::all_of(brick.indices,
                              [&](std::uint32_t index) { return index < brick.vertices.size(); }));
    CHECK(std::ranges::all_of(brick.vertices, [](const assets::GeometryVertex& vertex) {
        return Length(vertex.normal) == doctest::Approx(1.0F);
    }));
}

TEST_CASE("particles spawn around a flat circle and move in toward its middle") {
    FxEffect swirl = Puff(0.0F, 12);
    swirl.spawnRadius = 5.0F;
    swirl.radialSpeed = -10.0F;
    FxSystem fx = System(swirl);
    fx.Burst(*fx.EffectFor("puff"), {.position = kSwirlMiddle});
    const auto isAt = [](const QuadDraw& quad, float radius) {
        const Vector3 offset = Centre(quad) - kSwirlMiddle;
        return offset.y == doctest::Approx(0.0F) && Length(offset) == doctest::Approx(radius);
    };

    const std::vector<QuadDraw> spawned = Quads(fx);
    fx.Update(FloatMilliseconds{100.0F});
    const std::vector<QuadDraw> moved = Quads(fx);

    REQUIRE(spawned.size() == 12);
    CHECK(std::ranges::all_of(spawned, [&](const QuadDraw& quad) { return isAt(quad, 5.0F); }));
    CHECK(std::ranges::all_of(moved, [&](const QuadDraw& quad) { return isAt(quad, 4.0F); }));
}

TEST_CASE("bubbles and twinkles draw from their own textures") {
    FxEffect bubble = Puff(0.0F, 1);
    bubble.shape = FxShape::Bubble;
    FxEffect star = Puff(0.0F, 1);
    star.emitter = "star";
    star.shape = FxShape::Star;
    FxSystem fx({bubble, star}, kTextures);

    fx.Burst(*fx.EffectFor("puff"), {});
    fx.Burst(*fx.EffectFor("star"), {});

    std::vector<TextureId> textures;
    for (const QuadDraw& quad : Quads(fx)) {
        textures.push_back(quad.texture);
    }
    std::ranges::sort(textures);
    CHECK(textures == std::vector{kBubble, kStar});
}

TEST_CASE(
    "a bubble is clearer in the middle than at its rim, and a twinkle shines along its arms") {
    const assets::ImageAsset bubble = MakeBubbleTexture();
    const assets::ImageAsset star = MakeStarTexture();
    const std::uint32_t middle = bubble.widthPx / 2;
    const std::uint32_t rim = middle + (middle * 4 / 5);
    const std::uint32_t arm = star.widthPx / 2;

    CHECK(Alpha(bubble, middle, middle) < Alpha(bubble, rim, middle));
    CHECK(Alpha(bubble, 0, 0) == doctest::Approx(0.0F));
    CHECK(Alpha(star, arm, arm) > 0.9F);
    CHECK(Alpha(star, arm, 6) > 4.0F * Alpha(star, 10, 10));
}

TEST_CASE("particles grow, fade and die at the end of their life") {
    FxSystem fx = System(Puff(0.0F, 1));
    FxEmission emission;
    fx.Emit(*fx.EffectFor("puff"), {}, 0.0F, emission);

    const float young = Quads(fx).front().tint.a;
    fx.Update(FloatMilliseconds{500.0F});
    const QuadDraw middle = Quads(fx).front();
    fx.Update(FloatMilliseconds{501.0F});

    CHECK(young == doctest::Approx(0.0F));
    CHECK(middle.tint.a > 0.5F);
    CHECK(Length(middle.corners[0] - middle.corners[3]) > std::numbers::sqrt2_v<float> * 2.0F);
    CHECK(fx.Count() == 0);
}

TEST_CASE("the pool drops spawns past its capacity") {
    FxSystem fx = System(Puff(0.0F, 50), 20);
    FxEmission emission;

    fx.Emit(*fx.EffectFor("puff"), {}, 0.0F, emission);

    CHECK(fx.Count() == 20);
}

TEST_CASE("quads come back to front") {
    FxSystem fx = System(Puff(0.0F, 1));
    const FxEffectId puff = *fx.EffectFor("puff");
    for (const float z : {-40.0F, 30.0F, 0.0F}) {
        FxEmission emission;
        fx.Emit(puff, {.position = {.z = z}}, 0.0F, emission);
    }

    const std::vector<QuadDraw> quads = Quads(fx);

    REQUIRE(quads.size() == 3);
    CHECK(quads[0].corners[0].z > quads[1].corners[0].z);
    CHECK(quads[1].corners[0].z > quads[2].corners[0].z);
    CHECK(quads[0].texture == kSmoke);
}

bool IsPuffInCell(const assets::ImageAsset& atlas, std::size_t variant) {
    constexpr std::uint32_t kMiddle = kSmokeCellPx / 2;
    const Rect cell = SmokeCell(variant);
    const auto left = static_cast<std::uint32_t>(cell.x * static_cast<float>(atlas.widthPx));
    const auto top = static_cast<std::uint32_t>(cell.y * static_cast<float>(atlas.heightPx));
    return Alpha(atlas, left, top) == 0.0F &&
           Alpha(atlas, left + kSmokeCellPx - 1, top + kMiddle) == 0.0F &&
           Alpha(atlas, left + kMiddle, top + kMiddle) > 0.2F;
}

TEST_CASE("smoke puffs are dense in the middle and clear at their cell's edges") {
    const assets::ImageAsset atlas = MakeSmokeAtlas();

    REQUIRE(atlas.widthPx == kSmokeCellPx * 2);
    for (std::size_t variant = 0; variant < kSmokeVariants; ++variant) {
        CHECK(IsPuffInCell(atlas, variant));
    }
}

ParticleBank Bank() {
    return ParticleBank({.emitters = {{.name = "puff",
                                       .emitRate = 10.0F,
                                       .points = {{.z = 1.0F}},
                                       .particleLife = Milliseconds{1000},
                                       .radius = 1000.0F,
                                       .priority = 10}},
                         .emitterLooks = {ParticleLook{.texture = kOriginal}},
                         .slotCount = 2});
}

std::optional<EmitterHandle> RunPuff(ParticleBank& bank) {
    const std::optional<EmitterHandle> handle = bank.Start("puff", {});
    for (int frame = 0; frame < 9; ++frame) {
        bank.Update(FloatMilliseconds{10.0F});
    }
    return handle;
}

std::vector<QuadDraw> Quads(const ParticleBank& bank) {
    std::vector<QuadDraw> quads;
    bank.AppendQuads(kCamera, quads);
    return quads;
}

TEST_CASE("a bank draws an emitter's effect in place of its particles and keeps its timing") {
    FxSystem fx = System(Puff(100.0F));
    ParticleBank bank = Bank();
    bank.SetEffects(&fx);

    const std::optional<EmitterHandle> handle = RunPuff(bank);

    REQUIRE(handle);
    CHECK(Quads(bank).empty());
    CHECK(fx.Count() == 9);
    CHECK(bank.SpawnCount(*handle) == 1);
}

TEST_CASE("a bank without effects draws its own particles again") {
    FxSystem fx = System(Puff(100.0F));
    ParticleBank bank = Bank();
    bank.SetEffects(&fx);
    static_cast<void>(RunPuff(bank));

    bank.SetEffects(nullptr);

    const std::vector<QuadDraw> drawn = Quads(bank);
    REQUIRE(drawn.size() == 1);
    CHECK(drawn.front().texture == kOriginal);
}

}
}
