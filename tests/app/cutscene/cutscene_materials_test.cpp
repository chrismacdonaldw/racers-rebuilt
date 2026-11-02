#include "app/cutscene/cutscene_materials.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace rr::app {
namespace {

constexpr std::array<std::uint32_t, 2> kSlots{0, 1};
constexpr float kRed = 1.0F;
constexpr float kBlue = 0.5F;
constexpr float kUntouched = 0.0F;

WorldAnimations RedThenBlue() {
    return {{{.keys = {{.material = "red", .frame = 0}, {.material = "blue", .frame = 5}},
              .tracks = {{.firstKey = 0, .keyCount = 2, .duration = 10, .framesPerSecond = 100}}}}};
}

assets::NameMap<render::Material> Palette() {
    return {{"RED", {.tint = {.r = kRed}}}, {"BLUE", {.tint = {.r = kBlue}}}};
}

assets::CutsceneModel Bound(std::size_t slot, std::size_t table = 0) {
    return {.materials = {
                {.world = 0, .animation = 0, .track = 0, .slot = slot, .materialTable = table}}};
}

CutsceneMaterials Create(const assets::CutsceneFrame& frame, std::span<const ModelBinding> models) {
    Result<CutsceneMaterials> created =
        CutsceneMaterials::Create(frame, models, RedThenBlue(), Palette());
    REQUIRE(created.has_value());
    return std::move(*created);
}

std::array<float, 2> Shown(const CutsceneMaterials& materials, std::size_t model) {
    std::array<render::MeshDraw, 2> draws{};
    draws[0].material.tint.r = kUntouched;
    draws[1].material.tint.r = kUntouched;
    materials.Apply(model, draws, kSlots);
    return {draws[0].material.tint.r, draws[1].material.tint.r};
}

TEST_CASE("a bound track shows its keys in its slot at the track's own rate, looping") {
    const assets::CutsceneFrame frame{.models = {Bound(1)}};
    const std::array models{ModelBinding{.staticDraws = DrawSpan{.first = 0, .count = 2}}};
    CutsceneMaterials materials = Create(frame, models);

    materials.Advance(Milliseconds{10});
    CHECK(Shown(materials, 0) == std::array{kUntouched, kUntouched});

    materials.Begin(0);
    materials.Advance(Milliseconds{10});
    CHECK(Shown(materials, 0) == std::array{kUntouched, kRed});
    materials.Advance(Milliseconds{40});
    CHECK(Shown(materials, 0) == std::array{kUntouched, kBlue});
    materials.Advance(Milliseconds{50});
    CHECK(Shown(materials, 0) == std::array{kUntouched, kRed});
}

TEST_CASE("a track moves to the newest model that binds it, and the old one keeps its material") {
    const assets::CutsceneFrame frame{.models = {Bound(0), Bound(0)}};
    const std::array models{ModelBinding{.staticDraws = DrawSpan{.first = 0, .count = 2}},
                            ModelBinding{.jointed = 0}};
    CutsceneMaterials materials = Create(frame, models);

    materials.Begin(0);
    materials.Advance(Milliseconds{10});
    materials.Begin(1);
    materials.End(0);
    materials.Advance(Milliseconds{40});

    CHECK(Shown(materials, 0) == std::array{kRed, kUntouched});
    CHECK(Shown(materials, 1) == std::array{kBlue, kUntouched});
}

TEST_CASE("an ended model's track holds its key and resumes from its cursor on the next begin") {
    const assets::CutsceneFrame frame{.models = {Bound(1)}};
    const std::array models{ModelBinding{.staticDraws = DrawSpan{.first = 0, .count = 2}}};
    CutsceneMaterials materials = Create(frame, models);

    materials.Begin(0);
    materials.Advance(Milliseconds{10});
    materials.End(0);
    materials.Advance(Milliseconds{60});
    CHECK(Shown(materials, 0) == std::array{kUntouched, kRed});

    materials.Begin(0);
    materials.Advance(Milliseconds{40});
    CHECK(Shown(materials, 0) == std::array{kUntouched, kBlue});
}

TEST_CASE("models without an entity, and other tables, change nothing") {
    const assets::CutsceneFrame frame{.models = {Bound(1), Bound(1, 1)}};
    const std::array models{ModelBinding{},
                            ModelBinding{.staticDraws = DrawSpan{.first = 0, .count = 2}}};
    CutsceneMaterials materials = Create(frame, models);

    materials.Begin(0);
    materials.Begin(1);
    materials.Advance(Milliseconds{10});

    CHECK(Shown(materials, 0) == std::array{kUntouched, kUntouched});
    CHECK(Shown(materials, 1) == std::array{kUntouched, kUntouched});
}

TEST_CASE("a binding past its lists or showing a missing material fails") {
    const std::array models{ModelBinding{.jointed = 0}};
    assets::CutsceneFrame frame{.models = {Bound(0)}};
    frame.models[0].materials[0].track = 1;
    CHECK_FALSE(CutsceneMaterials::Create(frame, models, RedThenBlue(), Palette()).has_value());
    frame.models[0].materials[0].track = 0;
    frame.models[0].materials[0].world = 1;
    CHECK_FALSE(CutsceneMaterials::Create(frame, models, RedThenBlue(), Palette()).has_value());

    frame.models[0].materials[0].world = 0;
    const Result<CutsceneMaterials> missing =
        CutsceneMaterials::Create(frame, models, RedThenBlue(), {{"RED", {}}});
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().message.ends_with("unable to find material blue"));
}

}
}
