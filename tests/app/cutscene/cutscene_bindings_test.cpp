#include "app/cutscene/cutscene_bindings.hpp"

#include <doctest/doctest.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "cutscene_stage.hpp"

namespace rr::app {
namespace {

assets::CutsceneModel Named(std::string name, bool isJointed = false) {
    return {.model = assets::EntityByName{.name = std::move(name), .isJointed = isJointed}};
}

Result<CutsceneBindings> Bind(const assets::CutsceneFrame& frame) {
    const assets::WorldAsset world = test::StageWorld();
    const std::array worlds{&world};
    return BindCutscene(frame, worlds, test::StageStatics(), test::StageEntities());
}

std::string ErrorOf(const Result<CutsceneBindings>& bound) {
    return bound ? std::string{} : bound.error().message;
}

TEST_CASE("model events bind by name, ignoring case, and indexed models to their world's sprite") {
    const assets::CutsceneFrame frame{
        .models = {Named("Set"),
                   Named("GUY1", true),
                   {.model = assets::EntityByIndex{.world = 0, .entity = 0}}}};

    const Result<CutsceneBindings> bound = Bind(frame);

    REQUIRE(bound.has_value());
    REQUIRE(bound->models.size() == 3);
    const std::optional<DrawSpan> draws = bound->models[0].staticDraws;
    REQUIRE(draws.has_value());
    CHECK(draws.value_or(DrawSpan{}).count == 2);
    CHECK_FALSE(bound->models[0].jointed.has_value());
    CHECK(bound->models[1].jointed == 0);
    CHECK_FALSE(bound->models[2].staticDraws.has_value());
    CHECK_FALSE(bound->models[2].jointed.has_value());
    CHECK(bound->models[2].sprite.value_or(assets::WorldSprite{}).materialName == "star");
}

TEST_CASE("a model name no world has, or a sprite past its lists, fails") {
    CHECK(ErrorOf(Bind({.models = {Named("ghost")}})).contains("unable to find model ghost"));
    CHECK(ErrorOf(Bind({.models = {Named("ghost", true)}}))
              .contains("unable to find jointed model ghost"));
    CHECK(ErrorOf(Bind({.models = {{.model = assets::EntityByIndex{.world = 0, .entity = 1}}}}))
              .contains("sprite 1 of world 0 doesn't exist"));
    CHECK(ErrorOf(Bind({.models = {{.model = assets::EntityByIndex{.world = 1, .entity = 0}}}}))
              .contains("sprite 0 of world 1 doesn't exist"));
}

TEST_CASE("cameras follow their tracked node, or stand looking at their target") {
    const assets::CutsceneFrame frame{
        .cameras = {{.camera = "camera01", .part = 0}, {.camera = "WIDE"}}};

    const Result<CutsceneBindings> bound = Bind(frame);

    REQUIRE(bound.has_value());
    const CameraBinding& tracked = bound->cameras[0];
    CHECK(tracked.entity == 1);
    CHECK(tracked.node == 1);
    CHECK(tracked.verticalFovDegrees == 50.0F);
    CHECK(tracked.nearPlane == 2.0F);
    CHECK(tracked.farPlane == 300.0F);
    const CameraBinding& standing = bound->cameras[1];
    CHECK_FALSE(standing.entity.has_value());
    CHECK(standing.placement.position.x == -5.0F);
    const Vector3 facing = Rotate(standing.placement.rotation, {.x = 1.0F});
    CHECK(facing.x == doctest::Approx(1.0F));
}

TEST_CASE("a missing camera, or a part past the model's parts, fails at load") {
    CHECK(ErrorOf(Bind({.cameras = {{.camera = "nope"}}}))
              .contains("unable to find named camera nope"));

    assets::CutsceneModel model = Named("guy1", true);
    model.part = 1;
    CHECK(ErrorOf(Bind({.models = {model}})).contains("plays part 1 of guy1, which has 1"));
    CHECK(ErrorOf(Bind({.cameras = {{.camera = "Camera01", .part = -2}}})).contains("part -2"));
}

}
}
