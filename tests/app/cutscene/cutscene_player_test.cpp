#include "app/cutscene/cutscene_player.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "app/track_space.hpp"
#include "cutscene_stage.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kFrame{34};

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

sim::PartClock ClockOf(const CutscenePlayer& player, std::size_t entity) {
    const sim::EntityClock clock = player.Clocks()[entity];
    REQUIRE(clock.has_value());
    return clock.value_or(sim::PartClock{});
}

struct Stage {
    explicit Stage(assets::CutsceneFrame shown) : frame(std::move(shown)) {
        const assets::WorldAsset world = test::StageWorld();
        const std::array worlds{&world};
        Result<CutsceneBindings> bound =
            BindCutscene(frame, worlds, test::StageStatics(), entities);
        REQUIRE(bound.has_value());
        bindings = std::move(*bound);
    }

    CutscenePlayer Player(const WorldAnimations& animations = {},
                          const assets::NameMap<render::Material>& materials = {}) {
        Result<CutsceneMaterials> tracks =
            CutsceneMaterials::Create(frame, bindings.models, animations, materials);
        REQUIRE(tracks.has_value());
        return {frame, bindings, std::move(*tracks), entities, {.draws = statics, .slots = slots},
                false};
    }

    void Advance(CutscenePlayer& player, Milliseconds elapsed) {
        player.Advance(elapsed);
        jointed.Update(player.Clocks());
    }

    [[nodiscard]] std::vector<render::MeshDraw> Draws(const CutscenePlayer& player) const {
        std::vector<render::MeshDraw> draws;
        player.AppendDraws(jointed, draws);
        return draws;
    }

    assets::CutsceneFrame frame;
    sim::TrackEntitySources entities = test::StageEntities();
    std::array<render::MeshDraw, 2> statics{};
    std::array<std::uint32_t, 2> slots{0, 1};
    CutsceneBindings bindings;
    TrackEntityDraws jointed{{}, entities};
};

assets::CutsceneModel Model(const char* name, bool isJointed, assets::CutsceneSpan span) {
    return {.span = span, .model = assets::EntityByName{.name = name, .isJointed = isJointed}};
}

TEST_CASE("a static model draws only while its event runs, where the event places it") {
    assets::CutsceneModel set = Model("set", false, {.start = 10, .duration = 10});
    set.position = {.x = 1.0F, .y = 2.0F, .z = 3.0F};
    Stage stage({.length = 30, .models = {set}});
    CutscenePlayer player = stage.Player();

    stage.Advance(player, Milliseconds{300});
    CHECK(stage.Draws(player).empty());

    stage.Advance(player, kFrame);
    const std::vector<render::MeshDraw> shown = stage.Draws(player);
    REQUIRE(shown.size() == 2);
    CheckNear(shown[1].transform.position, ToEngine(set.position));
    CHECK_FALSE(shown[1].isStatic);

    stage.Advance(player, Milliseconds{340});
    CHECK(stage.Draws(player).empty());
}

TEST_CASE("a model event runs its material track in its own slot") {
    assets::CutsceneModel set = Model("set", false, {.start = 0, .duration = 30});
    set.materials = {{.world = 0, .animation = 0, .track = 0, .slot = 1}};
    Stage stage({.length = 30, .models = {set}});
    const WorldAnimations animations{
        {{.keys = {{.material = "dull", .frame = 0}, {.material = "shiny", .frame = 3}},
          .tracks = {{.firstKey = 0, .keyCount = 2, .duration = 6, .framesPerSecond = 30}}}}};
    const assets::NameMap<render::Material> materials{{"DULL", {.tint = {.r = 0.25F}}},
                                                      {"SHINY", {.tint = {.r = 0.75F}}}};
    CutscenePlayer player = stage.Player(animations, materials);

    stage.Advance(player, kFrame);
    const std::vector<render::MeshDraw> first = stage.Draws(player);
    REQUIRE(first.size() == 2);
    CHECK(first[1].material.tint.r == 0.25F);
    stage.Advance(player, Milliseconds{100});
    const std::vector<render::MeshDraw> shown = stage.Draws(player);
    REQUIRE(shown.size() == 2);
    CHECK(shown[0].material.tint.r == 1.0F);
    CHECK(shown[1].material.tint.r == 0.75F);
}

TEST_CASE("a jointed model is placed by its event and animates only while it plays a part") {
    assets::CutsceneModel guy = Model("guy1", true, {.start = 0, .duration = 15});
    guy.position = {.x = 7.0F};
    guy.part = 0;
    Stage stage(
        {.length = 30,
         .cameras = {{.span = {.start = 0, .duration = 30}, .camera = "Camera01", .part = 0}},
         .models = {guy}});
    CutscenePlayer player = stage.Player();

    stage.Advance(player, kFrame);
    stage.Advance(player, kFrame);
    CHECK(stage.entities.entities[0].placement.position.x == 7.0F);
    const float playing = ClockOf(player, 0).frame;
    CHECK(playing > 0.0F);
    CHECK(ClockOf(player, 1).part == 0);
    CHECK(ClockOf(player, 1).frame == 0.0F);

    stage.Advance(player, Milliseconds{500});
    const float ended = ClockOf(player, 0).frame;
    stage.Advance(player, Milliseconds{100});
    CHECK(ClockOf(player, 0).frame == ended);
}

TEST_CASE("the view looks through the oldest camera's tracked node, with its lens") {
    Stage stage({.length = 30,
                 .cameras = {{.span = {.start = 5, .duration = 25}, .camera = "Camera01"},
                             {.span = {.start = 6, .duration = 24}, .camera = "wide"}}});
    CutscenePlayer player = stage.Player();

    stage.Advance(player, kFrame);
    CHECK_FALSE(player.ViewCamera(player.Clocks()).has_value());

    stage.Advance(player, Milliseconds{200});
    const std::optional<render::Camera> view = player.ViewCamera(player.Clocks());
    REQUIRE(view.has_value());
    const render::Camera camera = view.value_or(render::Camera{});
    CheckNear(camera.transform.position, ToEngine({.x = 10.0F}));
    CHECK(camera.verticalFovDegrees == 50.0F);
    CHECK(camera.nearPlane == 2.0F);
    CHECK(camera.farPlane == 300.0F);
}

TEST_CASE("cutscene lights become renderer colours and engine directions") {
    Stage stage({.length = 30,
                 .ambientLights = {{.span = {.start = 0, .duration = 10}, .color = {255, 51, 0}}},
                 .directionalLights = {{.span = {.start = 0, .duration = 30},
                                        .color = {0, 0, 255},
                                        .direction = {.y = 1.0F}}}});
    CutscenePlayer player = stage.Player();
    std::vector<render::DirectionalLight> storage;

    stage.Advance(player, kFrame);
    const render::Lighting lit = player.Lighting(storage);
    CHECK(lit.ambient.r == doctest::Approx(1.0F));
    CHECK(lit.ambient.g == doctest::Approx(0.2F));
    CHECK(lit.ambient.b == doctest::Approx(0.0F));
    REQUIRE(lit.lights.size() == 1);
    CheckNear(lit.lights[0].direction, {.z = -1.0F});
    CHECK(lit.lights[0].color.b == doctest::Approx(1.0F));
    CHECK(lit.lights[0].color.r == doctest::Approx(0.0F));

    stage.Advance(player, Milliseconds{400});
    const render::Lighting dark = player.Lighting(storage);
    CHECK(dark.ambient.r == 0.0F);
    CHECK(dark.ambient.g == 0.0F);
    CHECK(dark.lights.size() == 1);
}

}
}
