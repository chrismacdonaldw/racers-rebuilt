#include "app/player_cameras.hpp"

#include <doctest/doctest.h>

namespace rr::app {
namespace {

TEST_CASE("one player's view fills the window") {
    const render::Viewport whole = PlayerViewport(0, 1);

    CHECK(whole.x == 0.0F);
    CHECK(whole.y == 0.0F);
    CHECK(whole.width == 1.0F);
    CHECK(whole.height == 1.0F);
}

TEST_CASE("two players split the window into a top and a bottom half") {
    const render::Viewport top = PlayerViewport(0, 2);
    const render::Viewport bottom = PlayerViewport(1, 2);

    CHECK(top.y == 0.0F);
    CHECK(top.height == 0.5F);
    CHECK(bottom.y == 0.5F);
    CHECK(bottom.height == 0.5F);
    CHECK(bottom.width == 1.0F);
}

TEST_CASE("a split view's field of view is 25 degrees narrower") {
    const render::Camera camera{.verticalFovDegrees = 60.0F};

    CHECK(PlayerLens(camera, 1).verticalFovDegrees == 60.0F);
    CHECK(PlayerLens(camera, 2).verticalFovDegrees == 35.0F);
}

TEST_CASE("player 1 views from the snapshot's camera and player 2 from the next") {
    const sim::RenderSnapshot snapshot{
        .camera = {.transform = {.position = {.x = 1.0F}}},
        .otherPlayerCameras = {{.transform = {.position = {.x = 2.0F}}}}};

    CHECK(PlayerCameraPose(snapshot, 0).position.x == 1.0F);
    CHECK(PlayerCameraPose(snapshot, 1).position.x == 2.0F);
}

}
}
